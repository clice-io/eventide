#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/attrs.h"
#include "kota/meta/annotation.h"
#include "kota/codec/fbs/fbs.h"
#include "flatbuffers/flatbuffers.h"

namespace kota::codec {

namespace {

using fbs::from_flatbuffer;
using fbs::to_flatbuffer;
using fbs::verify_flatbuffer;

struct inner_node {
    std::string name;
    std::vector<std::uint32_t> values;
};

struct kitchen_sink {
    std::int32_t id = 0;
    std::string title;
    std::vector<std::byte> blob;
    std::vector<std::string> tags;
    std::map<std::string, std::int32_t> counters;
    std::map<std::uint64_t, inner_node> nodes;
    std::optional<std::int64_t> maybe;
    std::variant<std::int32_t, std::string> either = 0;
    std::vector<inner_node> children;
    std::vector<std::optional<std::uint16_t>> sparse;
    std::tuple<std::int32_t, std::string> pair_like{0, ""};
};

kitchen_sink make_sample() {
    kitchen_sink value;
    value.id = 42;
    value.title = "sample";
    value.blob = {std::byte{0x01}, std::byte{0xFF}, std::byte{0x7E}};
    value.tags = {"alpha", "beta"};
    value.counters = {{"x", 1}, {"y", 2}};
    value.nodes[7] = inner_node{"seven", {1, 2, 3}};
    value.nodes[100] = inner_node{"hundred", {}};
    value.maybe = -5;
    value.either = std::string("payload");
    value.children = {inner_node{"c0", {9}}, inner_node{"c1", {8, 7}}};
    value.sparse = {std::uint16_t{4}, std::nullopt, std::uint16_t{6}};
    value.pair_like = {12, "tail"};
    return value;
}

struct failing_adapter {
    template <typename Vis>
    static bool serialize(Vis&, const std::int32_t&) {
        return scoped_context<rich_error>::fail(rich_error("adapter refused"));
    }

    template <typename Vis>
    static bool deserialize(Vis&, std::int32_t&) {
        return scoped_context<rich_error>::fail(rich_error("adapter refused"));
    }
};

struct failing_inner {
    meta::annotation<std::int32_t, meta::behavior::with<failing_adapter>> field;
};

struct failing_outer {
    failing_inner child;
    std::string tail;
};

struct long_double_holder {
    long double value = 0;
    std::vector<long double> values;
};

TEST_SUITE(serde_flatbuffers_robustness) {

TEST_CASE(verify_accepts_valid_buffer) {
    auto encoded = to_flatbuffer(make_sample());
    ASSERT_TRUE(encoded.has_value());
    EXPECT_TRUE(verify_flatbuffer<kitchen_sink>(std::span<const std::uint8_t>(*encoded)));

    kitchen_sink decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.id, 42);
    EXPECT_EQ(decoded.title, "sample");
    EXPECT_EQ(decoded.tags.size(), 2U);
    EXPECT_TRUE(decoded.counters == make_sample().counters);
    ASSERT_EQ(decoded.nodes.size(), 2U);
    EXPECT_EQ(decoded.nodes[7].name, "seven");
    EXPECT_TRUE(decoded.maybe.has_value());
    ASSERT_EQ(decoded.either.index(), 1U);
    EXPECT_EQ(std::get<1>(decoded.either), "payload");
    ASSERT_EQ(decoded.sparse.size(), 3U);
    EXPECT_FALSE(decoded.sparse[1].has_value());
}

TEST_CASE(verify_rejects_wrong_root_type_gracefully) {
    // A buffer encoded for one type read as a completely different type must
    // never read out of bounds; it may decode to defaults or fail, but the
    // verifier has to keep every access inside the buffer.
    struct other_shape {
        std::vector<std::string> a;
        std::map<std::uint32_t, std::vector<std::uint64_t>> b;
        std::string c;
    };

    auto encoded = to_flatbuffer(make_sample());
    ASSERT_TRUE(encoded.has_value());

    other_shape decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    (void)result;  // either outcome is fine — the run must be clean under ASan
}

TEST_CASE(truncated_buffers_never_crash) {
    auto encoded = to_flatbuffer(make_sample());
    ASSERT_TRUE(encoded.has_value());

    std::size_t rejected = 0;
    for(std::size_t len = 0; len < encoded->size(); ++len) {
        std::vector<std::uint8_t> truncated(encoded->begin(),
                                            encoded->begin() + static_cast<std::ptrdiff_t>(len));
        kitchen_sink decoded;
        auto result = from_flatbuffer(std::span<const std::uint8_t>(truncated), decoded);
        if(!result.has_value()) {
            ++rejected;
        }
        if(len < 8) {
            // Too short to even hold the root offset and identifier.
            EXPECT_FALSE(result.has_value());
        }
    }
    // Chopping into real data must be caught; only trailing alignment padding
    // may still verify, so the overwhelming majority of prefixes must fail.
    EXPECT_TRUE(rejected + 16 >= encoded->size());
}

TEST_CASE(bitflipped_buffers_never_crash) {
    auto encoded = to_flatbuffer(make_sample());
    ASSERT_TRUE(encoded.has_value());

    for(std::size_t i = 0; i < encoded->size(); ++i) {
        for(std::uint8_t pattern: {std::uint8_t{0xFF}, std::uint8_t{0x80}, std::uint8_t{0x01}}) {
            auto copy = *encoded;
            copy[i] ^= pattern;
            kitchen_sink decoded;
            auto result = from_flatbuffer(std::span<const std::uint8_t>(copy), decoded);
            (void)result;  // corrupt scalars may still decode; OOB must not happen
        }
    }
}

TEST_CASE(encode_failure_in_nested_table_propagates) {
    failing_outer input;
    input.tail = "t";

    auto encoded = to_flatbuffer(input);
    EXPECT_FALSE(encoded.has_value());
}

TEST_CASE(long_double_round_trip) {
    long_double_holder input;
    input.value = 3.5L;
    input.values = {1.25L, -2.5L, 1024.0L};

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    long_double_holder decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(static_cast<double>(decoded.value), 3.5);
    ASSERT_EQ(decoded.values.size(), 3U);
    EXPECT_EQ(static_cast<double>(decoded.values[0]), 1.25);
    EXPECT_EQ(static_cast<double>(decoded.values[1]), -2.5);
    EXPECT_EQ(static_cast<double>(decoded.values[2]), 1024.0);
}

};  // TEST_SUITE(serde_flatbuffers_robustness)

}  // namespace

}  // namespace kota::codec

#endif
