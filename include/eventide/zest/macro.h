#pragma once

#include "check.h"
#include "suite.h"
#include "trace.h"

#define TEST_SUITE(name) struct name##TEST : ::eventide::zest::TestSuiteDef<#name, name##TEST>

#define TEST_CASE(name, ...)                                                                       \
    void _register_##name() {                                                                      \
        constexpr auto file_name = std::source_location::current().file_name();                    \
        constexpr auto file_len = std::string_view(file_name).size();                              \
        (void)_register_suites<>;                                                                  \
        (void)_register_test_case<#name,                                                           \
                                  &Self::test_##name,                                              \
                                  ::eventide::fixed_string<file_len>(file_name),                   \
                                  std::source_location::current().line() __VA_OPT__(, )            \
                                      __VA_ARGS__>;                                                \
    }                                                                                              \
    void test_##name()

#define CLICE_CHECK_IMPL(condition, return_action)                                                 \
    do {                                                                                           \
        if(condition) [[unlikely]] {                                                               \
            ::eventide::zest::print_trace(std::source_location::current());                        \
            failure();                                                                             \
            return_action;                                                                         \
        }                                                                                          \
    } while(0)

#define ZEST_EXPECT_UNARY(expr, expectation, failure_pred, return_action)                          \
    do {                                                                                           \
        auto _failed = ([&](auto&& _expr) {                                                        \
            return ::eventide::zest::check_unary_failure((failure_pred),                           \
                                                         #expr,                                    \
                                                         (expectation),                            \
                                                         _expr);                                   \
        }((expr)));                                                                                \
        CLICE_CHECK_IMPL(_failed, return_action);                                                  \
    } while(0)

#define ZEST_EXPECT_BINARY(op_string, failure_pred, return_action, ...)                            \
    do {                                                                                           \
        auto _failed = ([&]<typename... _Args>(_Args&&... _args) {                                 \
            static_assert(sizeof...(_args) == 2,                                                   \
                          "EXPECT_EQ/EXPECT_NE/ASSERT_EQ/ASSERT_NE require exactly 2 arguments");  \
            auto _args_tuple = std::forward_as_tuple(std::forward<_Args>(_args)...);               \
            auto&& _lhs = std::get<0>(_args_tuple);                                                \
            auto&& _rhs = std::get<1>(_args_tuple);                                                \
            const auto _exprs = ::eventide::zest::parse_binary_exprs(#__VA_ARGS__);                \
            return ::eventide::zest::check_binary_failure((failure_pred),                          \
                                                          #op_string,                              \
                                                          _exprs.lhs,                              \
                                                          _exprs.rhs,                              \
                                                          _lhs,                                    \
                                                          _rhs);                                   \
        }(__VA_ARGS__));                                                                           \
        CLICE_CHECK_IMPL(_failed, return_action);                                                  \
    } while(0)

#define EXPECT_TRUE(expr) ZEST_EXPECT_UNARY(expr, "true", !(_expr), (void)0)
#define EXPECT_FALSE(expr) ZEST_EXPECT_UNARY(expr, "false", (_expr), (void)0)
#define EXPECT_EQ(...)                                                                             \
    ZEST_EXPECT_BINARY(==, !::eventide::zest::binary_equal(_lhs, _rhs), (void)0, __VA_ARGS__)
#define EXPECT_NE(...)                                                                             \
    ZEST_EXPECT_BINARY(!=, ::eventide::zest::binary_equal(_lhs, _rhs), (void)0, __VA_ARGS__)

#define ASSERT_TRUE(expr) ZEST_EXPECT_UNARY(expr, "true", !(_expr), return)
#define ASSERT_FALSE(expr) ZEST_EXPECT_UNARY(expr, "false", (_expr), return)
#define ASSERT_EQ(...)                                                                             \
    ZEST_EXPECT_BINARY(==, !::eventide::zest::binary_equal(_lhs, _rhs), return, __VA_ARGS__)
#define ASSERT_NE(...)                                                                             \
    ZEST_EXPECT_BINARY(!=, ::eventide::zest::binary_equal(_lhs, _rhs), return, __VA_ARGS__)

#define CO_ASSERT_TRUE(expr) ZEST_EXPECT_UNARY(expr, "true", !(_expr), co_return)
#define CO_ASSERT_FALSE(expr) ZEST_EXPECT_UNARY(expr, "false", (_expr), co_return)
#define CO_ASSERT_EQ(...)                                                                          \
    ZEST_EXPECT_BINARY(==, !::eventide::zest::binary_equal(_lhs, _rhs), co_return, __VA_ARGS__)
#define CO_ASSERT_NE(...)                                                                          \
    ZEST_EXPECT_BINARY(!=, ::eventide::zest::binary_equal(_lhs, _rhs), co_return, __VA_ARGS__)

#ifdef __cpp_exceptions

#define CAUGHT(expr)                                                                               \
    ([&]() {                                                                                       \
        try {                                                                                      \
            (expr);                                                                                \
            return false;                                                                          \
        } catch(...) {                                                                             \
            return true;                                                                           \
        }                                                                                          \
    }())

#define ZEST_EXPECT_THROWS(expr, expectation, failure_pred, return_action)                         \
    do {                                                                                           \
        auto _failed = ([&]() {                                                                    \
            return ::eventide::zest::check_throws_failure((failure_pred), #expr, (expectation));   \
        }());                                                                                      \
        CLICE_CHECK_IMPL(_failed, return_action);                                                  \
    } while(0)

#define EXPECT_THROWS(expr) ZEST_EXPECT_THROWS(expr, "throw exception", !CAUGHT(expr), (void)0)
#define EXPECT_NOTHROWS(expr) ZEST_EXPECT_THROWS(expr, "not throw exception", CAUGHT(expr), (void)0)

#endif
