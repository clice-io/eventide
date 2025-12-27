#pragma once

#include "suite.h"
#include "trace.h"

#define TEST_SUITE(name) struct name##TEST : zest::TestSuiteDef<#name, name##TEST>

#define TEST_CASE(name, ...)                                                                       \
    void _register_##name() {                                                                      \
        constexpr auto file_name = std::source_location::current().file_name();                    \
        constexpr auto file_len = std::string_view(file_name).size();                              \
        (void)_register_suites<>;                                                                  \
        (void)_register_test_case<#name,                                                           \
                                  &Self::test_##name,                                              \
                                  refl::fixed_string<file_len>(file_name),                         \
                                  std::source_location::current().line() __VA_OPT__(, )            \
                                      __VA_ARGS__>;                                                \
    }                                                                                              \
    void test_##name()

#define CLICE_CHECK_IMPL(condition, return_action)                                                 \
    do {                                                                                           \
        if(condition) [[unlikely]] {                                                               \
            auto trace = cpptrace::generate_trace();                                               \
            zest::print_trace(trace, std::source_location::current());                             \
            failure();                                                                             \
            return_action;                                                                         \
        }                                                                                          \
    } while(0)

#define EXPECT_TRUE(expr) CLICE_CHECK_IMPL(!(expr), (void)0)
#define EXPECT_FALSE(expr) CLICE_CHECK_IMPL((expr), (void)0)
#define EXPECT_EQ(lhs, rhs) CLICE_CHECK_IMPL((lhs) != (rhs), (void)0)
#define EXPECT_NE(lhs, rhs) CLICE_CHECK_IMPL((lhs) == (rhs), (void)0)

#define ASSERT_TRUE(expr) CLICE_CHECK_IMPL(!(expr), return)
#define ASSERT_FALSE(expr) CLICE_CHECK_IMPL((expr), return)
#define ASSERT_EQ(lhs, rhs) CLICE_CHECK_IMPL((lhs) != (rhs), return)
#define ASSERT_NE(lhs, rhs) CLICE_CHECK_IMPL((lhs) == (rhs), return)

#define CO_ASSERT_TRUE(expr) CLICE_CHECK_IMPL(!(expr), co_return)
#define CO_ASSERT_FALSE(expr) CLICE_CHECK_IMPL((expr), co_return)
#define CO_ASSERT_EQ(lhs, rhs) CLICE_CHECK_IMPL((lhs) != (rhs), co_return)
#define CO_ASSERT_NE(lhs, rhs) CLICE_CHECK_IMPL((lhs) == (rhs), co_return)
