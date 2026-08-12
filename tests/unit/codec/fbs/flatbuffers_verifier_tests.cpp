#if __has_include(<flatbuffers/flatbuffers.h>)

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/attrs.h"
#include "kota/codec/fbs/fbs.h"

// Corrupt-input coverage for the fbs verifier: the eager path
// (fbs::from_bytes) verifies every raw access while it reads, the zero-copy
// path (table_view::from_bytes) deep-verifies the buffer upfront. Either way
// the contract is the same: hostile bytes produce an error or an invalid
// view, never an out-of-bounds read — the ASan tree is the authority on the
// "never" part, these tests drive the inputs.

namespace kota::codec {

using namespace meta;

namespace {

using fbs::table_view;

struct inner {
    std::int32_t a = 0;
    std::string name;

    auto operator==(const inner&) const -> bool = default;
};

// No default member initializers: they would cost the type its trivial
// default constructor and with it the inline-struct layout under test.
struct point {
    std::int32_t x;
    std::int32_t y;

    auto operator==(const point&) const -> bool = default;
};

enum class grade : std::int32_t {
    low = 0,
    mid = 1,
    high = 2,
};

/// One field of every layout the verifier classifies: scalars, strings,
/// string/table/scalar/inline-struct vectors, a map, an optional, a variant,
/// bytes, and a tuple.
struct rich {
    std::int32_t id = 0;
    std::string title;
    std::vector<std::string> tags;
    std::vector<inner> items;
    std::vector<std::int32_t> nums;
    std::vector<point> pts;
    std::map<std::string, inner> index;
    std::optional<std::int32_t> opt;
    std::variant<std::int32_t, std::string, inner> which;
    std::vector<std::byte> blob;
    std::tuple<std::int32_t, std::string> pair_like;

    auto operator==(const rich&) const -> bool = default;
};

auto make_rich() -> rich {
    return rich{
        .id = 42,
        .title = "torture",
        .tags = {"alpha", "beta", "gamma"},
        .items = {{.a = 1, .name = "one"}, {.a = 2, .name = "two"}},
        .nums = {10, 20, 30, 40},
        .pts = {{.x = 1, .y = 2}, {.x = 3, .y = 4}},
        .index = {{"k1", {.a = 7, .name = "seven"}}, {"k2", {.a = 8, .name = "eight"}}},
        .opt = 99,
        .which = std::string("chosen"),
        .blob = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}},
        .pair_like = {5, "five"},
    };
}

/// The shapes rich leaves out: bare enum/char/byte cells, a long double
/// (double cell), an inline struct field, optional-of-table, an integer-keyed
/// map, nested vectors, a set, a monostate alternative, std::array behind a
/// tuple, and the two slot-rerouting behavior attrs (as, enum_string).
struct rich2 {
    grade level = grade::low;
    char tag = 'x';
    std::byte flag{0};
    long double ratio = 0.0L;
    point pos;
    std::optional<inner> extra;
    std::unordered_map<std::uint64_t, std::string> names;
    std::vector<std::vector<std::int32_t>> grid;
    std::set<std::int32_t> uniq;
    std::variant<std::monostate, inner> maybe;
    std::tuple<std::int32_t, std::array<std::int32_t, 3>> mixed;
    annotation<std::int32_t, behavior::as<std::int64_t>> widened{0};
    annotation<grade, behavior::enum_string<rename_policy::identity>> level_name{grade::low};

    auto operator==(const rich2&) const -> bool = default;
};

auto make_rich2() -> rich2 {
    return rich2{
        .level = grade::mid,
        .tag = 'k',
        .flag = std::byte{0x5A},
        .ratio = 2.5L,
        .pos = {.x = 3, .y = 4},
        .extra = inner{.a = 6, .name = "boxed"},
        .names = {{7u, "seven"}, {8u, "eight"}},
        .grid = {{1, 2}, {3}},
        .uniq = {5, 9},
        .maybe = inner{.a = 1, .name = "table payload"},
        .mixed = {11, {21, 22, 23}},
        .widened = {1234},
        .level_name = {grade::high},
    };
}

struct node {
    std::int32_t value = 0;
    std::unique_ptr<node> next;
};

/// weak_ptr is the third nullable smart pointer the encoder locks and writes
/// like shared_ptr; the classification (deep_clean_t) must peel it the same
/// way or the verifier checks the slot as a table offset instead of the
/// pointee's layout.
struct weak_holder {
    std::int32_t before = 0;
    std::weak_ptr<std::int32_t> num;
    std::string after;
};

auto make_chain(std::size_t depth) -> node {
    node head{.value = 0, .next = nullptr};
    node* tail = &head;
    for(std::size_t i = 1; i < depth; ++i) {
        tail->next = std::make_unique<node>();
        tail->next->value = static_cast<std::int32_t>(i);
        tail = tail->next.get();
    }
    return head;
}

/// Drives both hostile-input families over one fixture.
///
/// Truncation: chopping the tail removes data the root offset still points
/// into, so any prefix must either fail verification or — when only trailing
/// padding fell off — still decode to the original value.
///
/// Single-byte corruption: flipped values may still decode — a corrupted
/// scalar is in bounds — but must never crash. When the upfront walk accepts
/// a tampered buffer, `probe` exercises the view's offset-bearing accessors;
/// the ASan preset turns any escape into a hard failure.
template <typename T, typename Probe>
void expect_hostile_bytes_contained(const T& input, Probe&& probe) {
    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    for(std::size_t len = 8; len < encoded->size(); ++len) {
        auto span = std::span<const std::uint8_t>(encoded->data(), len);
        auto result = fbs::from_bytes<T>(span);
        if(result.has_value()) {
            EXPECT_EQ(*result, input);
        }
        auto root = table_view<T>::from_bytes(span);
        if(root.valid()) {
            probe(root);
        }
    }

    for(std::size_t pos = 0; pos < encoded->size(); ++pos) {
        auto tampered = *encoded;
        tampered[pos] ^= 0xFF;

        T sink{};
        [[maybe_unused]] auto result = fbs::from_bytes(tampered, sink);

        auto root = table_view<T>::from_bytes(tampered);
        if(root.valid()) {
            probe(root);
        }
    }
}

TEST_SUITE(serde_flatbuffers_verifier) {

TEST_CASE(rich_fixture_round_trips_both_paths) {
    const auto input = make_rich();
    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = fbs::from_bytes<rich>(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, input);

    auto root = table_view<rich>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&rich::id], 42);
    EXPECT_EQ(root[&rich::title], "torture");
    EXPECT_EQ(root[&rich::tags].size(), 3U);
    EXPECT_EQ(root[&rich::index]["k2"][&inner::name], "eight");
    EXPECT_EQ(root[&rich::which].get<1>(), "chosen");
}

TEST_CASE(rich2_fixture_round_trips_both_paths) {
    const auto input = make_rich2();
    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = fbs::from_bytes<rich2>(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, input);

    auto root = table_view<rich2>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_TRUE(root[&rich2::level] == grade::mid);
    EXPECT_EQ(root[&rich2::tag], 'k');
    EXPECT_TRUE(root[&rich2::flag] == std::byte{0x5A});
    EXPECT_EQ(root[&rich2::pos].y, 4);
    EXPECT_EQ(root[&rich2::extra][&inner::name], "boxed");
    EXPECT_EQ(root[&rich2::names][std::uint64_t{8}], "eight");
    EXPECT_EQ(root[&rich2::grid][0][1], 2);
    EXPECT_EQ(root[&rich2::uniq].size(), 2U);
    EXPECT_EQ(root[&rich2::maybe].index(), 1U);
    EXPECT_EQ(root[&rich2::maybe].get<1>()[&inner::a], 1);
    EXPECT_EQ(root[&rich2::mixed].get<1>().get<2>(), 23);
    // Behavior attrs reroute the slot: as<int64> reads back the widened cell,
    // enum_string reads back the enumerator's name.
    EXPECT_EQ(root[&rich2::widened], 1234);
    EXPECT_EQ(root[&rich2::level_name], "high");
}

TEST_CASE(monostate_alternative_round_trips_both_paths) {
    // A selected monostate travels as an empty table at the payload slot;
    // the views read the slot as a zero-size inline struct. Both must stay
    // in bounds.
    rich2 input = make_rich2();
    input.maybe = std::monostate{};

    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = fbs::from_bytes<rich2>(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, input);

    auto root = table_view<rich2>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&rich2::maybe].index(), 0U);
    [[maybe_unused]] auto unit = root[&rich2::maybe].get<0>();
}

TEST_CASE(weak_ptr_field_verifies_and_reads) {
    auto owner = std::make_shared<std::int32_t>(77);
    weak_holder input{.before = 5, .num = owner, .after = "tail"};

    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<weak_holder>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&weak_holder::before], 5);
    EXPECT_EQ(root[&weak_holder::num], 77);
    EXPECT_EQ(root[&weak_holder::after], "tail");

    // An expired weak_ptr leaves its slot absent; the view reads the default.
    input.num.reset();
    auto absent = fbs::to_bytes(input);
    ASSERT_TRUE(absent.has_value());

    auto root2 = table_view<weak_holder>::from_bytes(*absent);
    ASSERT_TRUE(root2.valid());
    EXPECT_EQ(root2[&weak_holder::num], 0);
}

TEST_CASE(undersized_buffers_are_rejected) {
    // Anything below root-uoffset + identifier cannot be a flatbuffer.
    std::vector<std::uint8_t> tiny(8, 0xAB);
    for(std::size_t len = 0; len < tiny.size(); ++len) {
        auto span = std::span<const std::uint8_t>(tiny.data(), len);
        auto result = fbs::from_bytes<rich>(span);
        ASSERT_FALSE(result.has_value());
        EXPECT_FALSE(table_view<rich>::from_bytes(span).valid());
    }
}

TEST_CASE(minimal_buffers_with_valid_identifier_are_rejected) {
    // Smallest spans that pass the size and identifier gates; the root
    // offset then points into the identifier or at the buffer end.
    const std::array<std::uint8_t, 8> into_identifier = {4, 0, 0, 0, 'E', 'V', 'T', 'O'};
    const std::array<std::uint8_t, 8> at_end = {8, 0, 0, 0, 'E', 'V', 'T', 'O'};

    for(const auto& raw: {into_identifier, at_end}) {
        auto span = std::span<const std::uint8_t>(raw.data(), raw.size());
        EXPECT_FALSE(fbs::from_bytes<rich>(span).has_value());
        EXPECT_FALSE(table_view<rich>::from_bytes(span).valid());
    }
}

TEST_CASE(wrong_identifier_is_rejected) {
    auto encoded = fbs::to_bytes(make_rich());
    ASSERT_TRUE(encoded.has_value());

    auto tampered = *encoded;
    tampered[4] ^= 0xFF;

    EXPECT_FALSE(fbs::from_bytes<rich>(tampered).has_value());
    EXPECT_FALSE(table_view<rich>::from_bytes(tampered).valid());
}

TEST_CASE(out_of_bounds_root_offset_is_rejected) {
    auto encoded = fbs::to_bytes(make_rich());
    ASSERT_TRUE(encoded.has_value());

    auto tampered = *encoded;
    tampered[0] = 0xFF;
    tampered[1] = 0xFF;
    tampered[2] = 0xFF;
    tampered[3] = 0x7F;

    EXPECT_FALSE(fbs::from_bytes<rich>(tampered).has_value());
    EXPECT_FALSE(table_view<rich>::from_bytes(tampered).valid());
}

TEST_CASE(hostile_bytes_never_read_out_of_bounds_rich) {
    expect_hostile_bytes_contained(make_rich(), [](const table_view<rich>& root) {
        [[maybe_unused]] auto title = root[&rich::title];
        auto tags = root[&rich::tags];
        for(std::size_t i = 0; i < tags.size(); ++i) {
            [[maybe_unused]] auto tag = tags[i];
        }
        auto items = root[&rich::items];
        for(std::size_t i = 0; i < items.size(); ++i) {
            [[maybe_unused]] auto name = items[i][&inner::name];
        }
        [[maybe_unused]] auto hit = root[&rich::index]["k1"];
        [[maybe_unused]] auto chosen = root[&rich::which].get<1>();
        [[maybe_unused]] auto second = root[&rich::pair_like].get<1>();
    });
}

TEST_CASE(hostile_bytes_never_read_out_of_bounds_rich2) {
    expect_hostile_bytes_contained(make_rich2(), [](const table_view<rich2>& root) {
        [[maybe_unused]] auto level = root[&rich2::level];
        [[maybe_unused]] auto pos = root[&rich2::pos];
        [[maybe_unused]] auto extra_name = root[&rich2::extra][&inner::name];
        [[maybe_unused]] auto lookup = root[&rich2::names][std::uint64_t{7}];
        auto grid = root[&rich2::grid];
        for(std::size_t i = 0; i < grid.size(); ++i) {
            auto row = grid[i];
            for(std::size_t j = 0; j < row.size(); ++j) {
                [[maybe_unused]] auto cell = row[j];
            }
        }
        [[maybe_unused]] auto uniq_size = root[&rich2::uniq].size();
        [[maybe_unused]] auto payload = root[&rich2::maybe].get<1>();
        [[maybe_unused]] auto arr = root[&rich2::mixed].get<1>().get<0>();
        [[maybe_unused]] auto widened = root[&rich2::widened];
        [[maybe_unused]] auto level_name = root[&rich2::level_name];
    });
}

TEST_CASE(recursion_depth_boundary) {
    // The verifier's depth cap (flatbuffers default 64) is also what
    // terminates cyclic offsets: a cycle is just an infinitely deep chain.
    // Straddle the cap so a change to per-table depth cost surfaces here.
    auto shallow = fbs::to_bytes(make_chain(60));
    ASSERT_TRUE(shallow.has_value());
    node shallow_out{};
    EXPECT_TRUE(fbs::from_bytes(*shallow, shallow_out).has_value());
    EXPECT_TRUE(table_view<node>::from_bytes(*shallow).valid());

    auto deep = fbs::to_bytes(make_chain(70));
    ASSERT_TRUE(deep.has_value());
    node deep_out{};
    EXPECT_FALSE(fbs::from_bytes(*deep, deep_out).has_value());
    EXPECT_FALSE(table_view<node>::from_bytes(*deep).valid());
}

TEST_CASE(byte_span_overload_rejects_hostile_input_too) {
    auto encoded = fbs::to_bytes(make_rich());
    ASSERT_TRUE(encoded.has_value());

    auto tampered = *encoded;
    tampered[0] = 0xFF;
    tampered[1] = 0xFF;
    tampered[2] = 0xFF;
    tampered[3] = 0x7F;

    auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(tampered.data()),
                                            tampered.size());
    rich sink{};
    EXPECT_FALSE(fbs::from_bytes(bytes, sink).has_value());
    EXPECT_FALSE(table_view<rich>::from_bytes(bytes).valid());
}

};  // TEST_SUITE(serde_flatbuffers_verifier)

}  // namespace

}  // namespace kota::codec

#endif
