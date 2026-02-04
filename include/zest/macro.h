#pragma once

#include <format>
#include <optional>
#include <print>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

#include "suite.h"
#include "trace.h"

namespace zest::detail {

template <typename T>
concept Formattable = std::formattable<T, char>;

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
constexpr inline bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

template <typename T>
inline std::string pretty_dump(const T& value) {
    if constexpr(is_optional_v<T>) {
        if(!value) {
            return std::string("nullopt");
        }
        return pretty_dump(*value);
    } else {
        if constexpr(Formattable<T>) {
            return std::format("{}", value);
        } else {
            return std::string("<unformattable>");
        }
    }
}

template <typename V>
inline bool check_unary_failure(bool failure,
                                std::string_view expr,
                                std::string_view expectation,
                                const V& value,
                                std::source_location loc = std::source_location::current()) {
    if(failure) {
        std::println("[ expect ] {} (expected {})", expr, expectation);
        std::println("           got: {}", pretty_dump(value));
        std::println("           at {}:{}", loc.file_name(), loc.line());
    }
    return failure;
}

template <typename L, typename R>
inline bool check_binary_failure(bool failure,
                                 std::string_view op,
                                 std::string_view lhs_expr,
                                 std::string_view rhs_expr,
                                 const L& lhs,
                                 const R& rhs,
                                 std::source_location loc = std::source_location::current()) {
    if(failure) {
        std::println("[ expect ] {} {} {}", lhs_expr, op, rhs_expr);
        std::println("           lhs: {}", pretty_dump(lhs));
        std::println("           rhs: {}", pretty_dump(rhs));
        std::println("           at {}:{}", loc.file_name(), loc.line());
    }
    return failure;
}

#ifdef __cpp_exceptions

inline bool check_throws_failure(bool failure,
                                 std::string_view expr,
                                 std::string_view expectation,
                                 std::source_location loc = std::source_location::current()) {
    if(failure) {
        std::println("[ expect ] {} (expected {})", expr, expectation);
        std::println("           at {}:{}", loc.file_name(), loc.line());
    }
    return failure;
}
#endif

}  // namespace zest::detail

#define TEST_SUITE(name) struct name##TEST : zest::TestSuiteDef<#name, name##TEST>

#define TEST_CASE(name, ...)                                                                       \
    void _register_##name() {                                                                      \
        constexpr auto file_name = std::source_location::current().file_name();                    \
        constexpr auto file_len = std::string_view(file_name).size();                              \
        (void)_register_suites<>;                                                                  \
        (void)_register_test_case<#name,                                                           \
                                  &Self::test_##name,                                              \
                                  zest::fixed_string<file_len>(file_name),                         \
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

#define ZEST_EXPECT_UNARY(expr, expectation, failure_pred, return_action)                          \
    do {                                                                                           \
        auto _failed = ([&](auto&& _expr) {                                                        \
            return zest::detail::check_unary_failure((failure_pred), #expr, (expectation), _expr); \
        }((expr)));                                                                                \
        CLICE_CHECK_IMPL(_failed, return_action);                                                  \
    } while(0)

#define ZEST_EXPECT_BINARY(lhs, rhs, op_string, failure_pred, return_action)                       \
    do {                                                                                           \
        auto _failed = ([&](auto&& _lhs, auto&& _rhs) {                                            \
            return zest::detail::check_binary_failure((failure_pred),                              \
                                                      #op_string,                                  \
                                                      #lhs,                                        \
                                                      #rhs,                                        \
                                                      _lhs,                                        \
                                                      _rhs);                                       \
        }((lhs), (rhs)));                                                                          \
        CLICE_CHECK_IMPL(_failed, return_action);                                                  \
    } while(0)

#define EXPECT_TRUE(expr) ZEST_EXPECT_UNARY(expr, "true", !(_expr), (void)0)
#define EXPECT_FALSE(expr) ZEST_EXPECT_UNARY(expr, "false", (_expr), (void)0)
#define EXPECT_EQ(lhs, rhs) ZEST_EXPECT_BINARY(lhs, rhs, ==, (_lhs) != (_rhs), (void)0)
#define EXPECT_NE(lhs, rhs) ZEST_EXPECT_BINARY(lhs, rhs, !=, (_lhs) == (_rhs), (void)0)

#define ASSERT_TRUE(expr) ZEST_EXPECT_UNARY(expr, "true", !(_expr), return)
#define ASSERT_FALSE(expr) ZEST_EXPECT_UNARY(expr, "false", (_expr), return)
#define ASSERT_EQ(lhs, rhs) ZEST_EXPECT_BINARY(lhs, rhs, ==, (_lhs) != (_rhs), return)
#define ASSERT_NE(lhs, rhs) ZEST_EXPECT_BINARY(lhs, rhs, !=, (_lhs) == (_rhs), return)

#define CO_ASSERT_TRUE(expr) ZEST_EXPECT_UNARY(expr, "true", !(_expr), co_return)
#define CO_ASSERT_FALSE(expr) ZEST_EXPECT_UNARY(expr, "false", (_expr), co_return)
#define CO_ASSERT_EQ(lhs, rhs) ZEST_EXPECT_BINARY(lhs, rhs, ==, (_lhs) != (_rhs), co_return)
#define CO_ASSERT_NE(lhs, rhs) ZEST_EXPECT_BINARY(lhs, rhs, !=, (_lhs) == (_rhs), co_return)

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
            return zest::detail::check_throws_failure((failure_pred), #expr, (expectation));       \
        }());                                                                                      \
        CLICE_CHECK_IMPL(_failed, return_action);                                                  \
    } while(0)

#define EXPECT_THROWS(expr) ZEST_EXPECT_THROWS(expr, "throw exception", !CAUGHT(expr), (void)0)
#define EXPECT_NOTHROWS(expr) ZEST_EXPECT_THROWS(expr, "not throw exception", CAUGHT(expr), (void)0)

#endif
