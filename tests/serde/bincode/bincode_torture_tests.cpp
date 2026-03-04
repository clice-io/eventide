#include <string>
#include <variant>

#include "eventide/serde/bincode.h"
#include "eventide/zest/zest.h"

#include "../types.h"

namespace eventide::serde {

namespace {

using bincode::from_bytes;
using bincode::to_bytes;
using test_types::UltimateTest;

auto roundtrip(const UltimateTest& input) -> std::expected<UltimateTest, bincode::error_kind> {
    auto encoded = to_bytes(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(bincode::error_kind::invalid_state);
    }

    return from_bytes<UltimateTest>(*encoded);
}

TEST_SUITE(serde_bincode_torture) {

TEST_CASE(ultimate_roundtrip_bincode) {
    auto input = test_types::make_ultimate();
    auto output = roundtrip(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(input, *output);
}

TEST_CASE(variant_and_nullables_roundtrip_bincode) {
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

};  // TEST_SUITE(serde_bincode_torture)

}  // namespace

}  // namespace eventide::serde
