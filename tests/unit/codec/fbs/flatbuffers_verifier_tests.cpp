#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/fbs/fbs.h"

// Corrupt-input coverage for the fbs verifier: the eager path
// (fbs::from_bytes) verifies every raw access while it reads, the zero-copy
// path (table_view::from_bytes) deep-verifies the buffer upfront. Either way
// the contract is the same: hostile bytes produce an error or an invalid
// view, never an out-of-bounds read — the ASan tree is the authority on the
// "never" part, these tests drive the inputs.

namespace kota::codec {

namespace {

using fbs::table_view;

struct inner {
    std::int32_t a = 0;
    std::string name;

    auto operator==(const inner&) const -> bool = default;
};

struct point {
    std::int32_t x = 0;
    std::int32_t y = 0;

    auto operator==(const point&) const -> bool = default;
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

struct node {
    std::int32_t value = 0;
    std::unique_ptr<node> next;
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

TEST_SUITE(serde_flatbuffers_verifier) {

TEST_CASE(rich_fixture_round_trips_both_paths) {
    const auto input = make_rich();
    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = fbs::from_bytes<rich>(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(*decoded == input);

    auto root = table_view<rich>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&rich::id], 42);
    EXPECT_EQ(root[&rich::title], "torture");
    EXPECT_EQ(root[&rich::tags].size(), 3U);
    EXPECT_EQ(root[&rich::index]["k2"][&inner::name], "eight");
    EXPECT_EQ(root[&rich::which].get<1>(), "chosen");
}

TEST_CASE(undersized_buffers_are_rejected) {
    // Anything below root-uoffset + identifier cannot be a flatbuffer. The
    // pre-verifier decoder read the identifier at [4, 8) unconditionally —
    // a 3-byte file was a heap-buffer-overflow.
    std::vector<std::uint8_t> tiny(8, 0xAB);
    for(std::size_t len = 0; len < tiny.size(); ++len) {
        auto span = std::span<const std::uint8_t>(tiny.data(), len);
        auto result = fbs::from_bytes<rich>(span);
        ASSERT_FALSE(result.has_value());
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

TEST_CASE(truncation_never_reads_out_of_bounds) {
    // Chopping the tail removes table/vector/string data the root offset
    // still points into. Any prefix must either fail verification or — when
    // only trailing padding fell off — still decode to the original value.
    const auto input = make_rich();
    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    for(std::size_t len = 8; len < encoded->size(); ++len) {
        auto span = std::span<const std::uint8_t>(encoded->data(), len);

        auto result = fbs::from_bytes<rich>(span);
        if(result.has_value()) {
            EXPECT_TRUE(*result == input);
        }

        auto root = table_view<rich>::from_bytes(span);
        if(root.valid()) {
            EXPECT_EQ(root[&rich::id], 42);
        }
    }
}

TEST_CASE(single_word_corruption_never_reads_out_of_bounds) {
    // The report's original repro: a valid buffer with one 32-bit word
    // flipped passed the shallow root check and SEGV'd during decode. Both
    // paths must now survive every position; flipped values are allowed to
    // decode (a corrupted scalar is still in bounds), just never to crash.
    // The ASan preset turns any regression here into a hard failure.
    const auto input = make_rich();
    auto encoded = fbs::to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    for(std::size_t pos = 0; pos + 4 <= encoded->size(); pos += 4) {
        auto tampered = *encoded;
        tampered[pos] ^= 0xFF;
        tampered[pos + 1] ^= 0xFF;
        tampered[pos + 2] ^= 0xFF;
        tampered[pos + 3] ^= 0xFF;

        rich sink{};
        [[maybe_unused]] auto result = fbs::from_bytes(tampered, sink);

        auto root = table_view<rich>::from_bytes(tampered);
        if(root.valid()) {
            // The walk accepted the buffer, so every view access must be
            // safe; exercise the layouts that involve offsets.
            [[maybe_unused]] auto title = root[&rich::title];
            [[maybe_unused]] auto tags = root[&rich::tags];
            for(std::size_t i = 0; i < tags.size(); ++i) {
                [[maybe_unused]] auto tag = tags[i];
            }
            [[maybe_unused]] auto items = root[&rich::items];
            for(std::size_t i = 0; i < items.size(); ++i) {
                [[maybe_unused]] auto name = items[i][&inner::name];
            }
            [[maybe_unused]] auto hit = root[&rich::index]["k1"];
            [[maybe_unused]] auto chosen = root[&rich::which].get<1>();
            [[maybe_unused]] auto second = root[&rich::pair_like].get<1>();
        }
    }
}

TEST_CASE(recursion_depth_is_capped) {
    // The depth cap is also what terminates cyclic offsets: a cycle is just
    // an infinitely deep chain.
    auto shallow = fbs::to_bytes(make_chain(16));
    ASSERT_TRUE(shallow.has_value());
    node shallow_out{};
    EXPECT_TRUE(fbs::from_bytes(*shallow, shallow_out).has_value());
    EXPECT_TRUE(table_view<node>::from_bytes(*shallow).valid());

    auto deep = fbs::to_bytes(make_chain(100));
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
