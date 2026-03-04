#if __has_include(<flatbuffers/flexbuffers.h>) && __has_include(<flatbuffers/flatbuffers.h>)

#include <string>
#include <variant>

#include "eventide/serde/flatbuffers/binary.h"
#include "eventide/serde/flatbuffers/flex_deserializer.h"
#include "eventide/serde/flatbuffers/flex_serializer.h"
#include "eventide/zest/zest.h"

#include "../types.h"

namespace eventide::serde {

namespace {

using test_types::UltimateTest;

auto roundtrip_flex(const UltimateTest& input) -> std::expected<UltimateTest, flex::error_code> {
    auto encoded = flex::to_flatbuffer(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(flex::error_code::invalid_state);
    }

    return flex::from_flatbuffer<UltimateTest>(*encoded);
}

auto roundtrip_binary(const UltimateTest& input)
    -> std::expected<UltimateTest, flatbuffers::binary::object_error_code> {
    auto encoded = flatbuffers::binary::to_flatbuffer(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(flatbuffers::binary::object_error_code::invalid_state);
    }

    return flatbuffers::binary::from_flatbuffer<UltimateTest>(*encoded);
}

TEST_SUITE(serde_flatbuffers_torture) {

TEST_CASE(ultimate_roundtrip_flex) {
    {
        auto input = test_types::make_ultimate();
        auto output = roundtrip_flex(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = std::monostate{};
        input.nullables.opt_value.reset();
        input.nullables.heap_allocated.reset();
        auto output = roundtrip_flex(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = 123;
        auto output = roundtrip_flex(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = std::string("variant-text");
        auto output = roundtrip_flex(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }
}

TEST_CASE(ultimate_roundtrip_binary) {
    {
        auto input = test_types::make_ultimate();
        auto output = roundtrip_binary(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = std::monostate{};
        input.nullables.opt_value.reset();
        input.nullables.heap_allocated.reset();
        auto output = roundtrip_binary(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = 123;
        auto output = roundtrip_binary(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = std::string("variant-text");
        auto output = roundtrip_binary(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }
}

};  // TEST_SUITE(serde_flatbuffers_torture)

}  // namespace

}  // namespace eventide::serde

#endif
