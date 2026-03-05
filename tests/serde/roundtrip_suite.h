#pragma once

/// Shared roundtrip test macros for serde backends.
///
/// These macros generate standard roundtrip test cases.
/// They must be used inside a TEST_SUITE block where `roundtrip(input)` is callable.
///
/// Usage:
///   auto roundtrip = []<typename T>(const T& input) { ... };
///   SERDE_TEST_ULTIMATE_ROUNDTRIP(roundtrip)
///   SERDE_TEST_VARIANT_NULLABLES_ROUNDTRIP(roundtrip)
///   SERDE_TEST_SCALARS_ROUNDTRIP(roundtrip)
///   SERDE_TEST_NESTED_CONTAINERS_ROUNDTRIP(roundtrip)
///   SERDE_TEST_EMPTY_CONTAINERS_ROUNDTRIP(roundtrip)

#include "types.h"

#define SERDE_TEST_ULTIMATE_ROUNDTRIP(rt)                                                          \
    {                                                                                              \
        auto input = test_types::make_ultimate();                                                  \
        auto output = rt(input);                                                                   \
        ASSERT_TRUE(output.has_value());                                                           \
        EXPECT_EQ(input, *output);                                                                 \
    }

#define SERDE_TEST_VARIANT_NULLABLES_ROUNDTRIP(rt)                                                 \
    {                                                                                              \
        auto input = test_types::make_ultimate();                                                  \
        input.adts.multi_variant = std::monostate{};                                               \
        input.nullables.opt_value.reset();                                                         \
        input.nullables.heap_allocated.reset();                                                    \
        auto output = rt(input);                                                                   \
        ASSERT_TRUE(output.has_value());                                                           \
        EXPECT_EQ(input, *output);                                                                 \
    }                                                                                              \
    {                                                                                              \
        auto input = test_types::make_ultimate();                                                  \
        input.adts.multi_variant = 123;                                                            \
        auto output = rt(input);                                                                   \
        ASSERT_TRUE(output.has_value());                                                           \
        EXPECT_EQ(input, *output);                                                                 \
    }                                                                                              \
    {                                                                                              \
        auto input = test_types::make_ultimate();                                                  \
        input.adts.multi_variant = std::string("variant-text");                                    \
        auto output = rt(input);                                                                   \
        ASSERT_TRUE(output.has_value());                                                           \
        EXPECT_EQ(input, *output);                                                                 \
    }

#define SERDE_TEST_SCALARS_ROUNDTRIP(rt)                                                           \
    {                                                                                              \
        auto input = test_types::make_scalars();                                                   \
        auto output = rt(input);                                                                   \
        ASSERT_TRUE(output.has_value());                                                           \
        EXPECT_EQ(input, *output);                                                                 \
    }

#define SERDE_TEST_NESTED_CONTAINERS_ROUNDTRIP(rt)                                                 \
    {                                                                                              \
        auto input = test_types::make_nested_containers();                                         \
        auto output = rt(input);                                                                   \
        ASSERT_TRUE(output.has_value());                                                           \
        EXPECT_EQ(input, *output);                                                                 \
    }

#define SERDE_TEST_EMPTY_CONTAINERS_ROUNDTRIP(rt)                                                  \
    {                                                                                              \
        auto input = test_types::make_empty_containers();                                          \
        auto output = rt(input);                                                                   \
        ASSERT_TRUE(output.has_value());                                                           \
        EXPECT_EQ(input, *output);                                                                 \
    }
