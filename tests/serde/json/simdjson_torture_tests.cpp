#include "eventide/serde/json/simd_deserializer.h"
#include "eventide/serde/json/simd_serializer.h"
#include "eventide/zest/zest.h"

#include <string>
#include <variant>

#include "../types.h"

namespace eventide::serde {

namespace {

using json::simd::from_json;
using json::simd::to_json;
using test_types::UltimateTest;

auto roundtrip(const UltimateTest& input) -> std::expected<UltimateTest, json::error_kind> {
    auto encoded = to_json(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }

    return from_json<UltimateTest>(*encoded);
}

TEST_SUITE(serde_simdjson_torture) {

TEST_CASE(ultimate_roundtrip_json_simd) {
    auto input = test_types::make_ultimate();
    auto output = roundtrip(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(input, *output);
}

TEST_CASE(variant_and_nullables_roundtrip_json_simd) {
    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = std::monostate{};
        input.nullables.opt_value.reset();
        input.nullables.heap_allocated.reset();
        auto output = roundtrip(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = 123;
        auto output = roundtrip(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }

    {
        auto input = test_types::make_ultimate();
        input.adts.multi_variant = std::string("variant-text");
        auto output = roundtrip(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(input, *output);
    }
}

};  // TEST_SUITE(serde_simdjson_torture)

}  // namespace

}  // namespace eventide::serde
