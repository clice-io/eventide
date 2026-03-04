#if __has_include(<toml++/toml.hpp>)

#include <string>
#include <variant>

#include "eventide/serde/toml.h"
#include "eventide/zest/zest.h"

#include "../types.h"

namespace eventide::serde {

namespace {

using test_types::UltimateTest;
using toml::parse;
using toml::to_string;

auto roundtrip(const UltimateTest& input) -> std::expected<UltimateTest, toml::error_kind> {
    auto encoded = to_string(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }

    return parse<UltimateTest>(*encoded);
}

TEST_SUITE(serde_toml_torture) {

TEST_CASE(ultimate_roundtrip_toml) {
    auto input = test_types::make_ultimate();
    auto output = roundtrip(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(input, *output);
}

TEST_CASE(variant_and_nullables_roundtrip_toml) {
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

};  // TEST_SUITE(serde_toml_torture)

}  // namespace

}  // namespace eventide::serde

#endif
