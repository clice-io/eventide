#pragma once

#include <concepts>
#include <expected>
#include <format>
#include <optional>
#include <print>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "suite.h"
#include "trace.h"

namespace eventide::zest {

template <typename T>
concept Formattable = std::formattable<T, char>;

template <typename... Ts>
constexpr inline bool dependent_false_v = false;

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
constexpr inline bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

template <typename T>
struct is_expected : std::false_type {};

template <typename T, typename E>
struct is_expected<std::expected<T, E>> : std::true_type {};

template <typename T>
constexpr inline bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

template <typename T>
inline std::string pretty_dump(const T& value) {
    if constexpr(is_optional_v<T>) {
        if(!value) {
            return std::string("nullopt");
        }
        return eventide::zest::pretty_dump(*value);
    } else if constexpr(is_expected_v<T>) {
        if(value.has_value()) {
            return std::format("expected({})", eventide::zest::pretty_dump(*value));
        }
        return std::format("unexpected({})", eventide::zest::pretty_dump(value.error()));
    } else {
        if constexpr(Formattable<T>) {
            return std::format("{}", value);
        } else {
            return std::string("<unformattable>");
        }
    }
}

struct binary_expr_pair {
    std::string_view lhs;
    std::string_view rhs;
};

binary_expr_pair parse_binary_exprs(std::string_view exprs);

template <typename L, typename R>
inline bool binary_equal(const L& lhs, const R& rhs) {
    if constexpr(is_expected_v<L> && !is_expected_v<R>) {
        if(!lhs.has_value()) {
            return false;
        }
        if constexpr(requires {
                         { *lhs == rhs } -> std::convertible_to<bool>;
                     }) {
            return static_cast<bool>(*lhs == rhs);
        } else {
            static_assert(dependent_false_v<L, R>,
                          "EXPECT_EQ/ASSERT_EQ: expected value and rhs are not comparable");
            return false;
        }
    } else if constexpr(!is_expected_v<L> && is_expected_v<R>) {
        if(!rhs.has_value()) {
            return false;
        }
        if constexpr(requires {
                         { lhs == *rhs } -> std::convertible_to<bool>;
                     }) {
            return static_cast<bool>(lhs == *rhs);
        } else {
            static_assert(dependent_false_v<L, R>,
                          "EXPECT_EQ/ASSERT_EQ: lhs and expected value are not comparable");
            return false;
        }
    } else if constexpr(requires {
                            { lhs == rhs } -> std::convertible_to<bool>;
                        }) {
        return static_cast<bool>(lhs == rhs);
    } else {
        static_assert(dependent_false_v<L, R>, "EXPECT_EQ/ASSERT_EQ: operands are not comparable");
        return false;
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
        std::println("           got: {}", eventide::zest::pretty_dump(value));
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
        std::println("           lhs: {}", eventide::zest::pretty_dump(lhs));
        std::println("           rhs: {}", eventide::zest::pretty_dump(rhs));
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

}  // namespace eventide::zest

#define TEST_SUITE(name) struct name##TEST : ::eventide::zest::TestSuiteDef<#name, name##TEST>

#define TEST_CASE(name, ...)                                                                       \
    void _register_##name() {                                                                      \
        constexpr auto file_name = std::source_location::current().file_name();                    \
        constexpr auto file_len = std::string_view(file_name).size();                              \
        (void)_register_suites<>;                                                                  \
        (void)_register_test_case<#name,                                                           \
                                  &Self::test_##name,                                              \
                                  ::eventide::zest::fixed_string<file_len>(file_name),                       \
                                  std::source_location::current().line() __VA_OPT__(, )            \
                                      __VA_ARGS__>;                                                \
    }                                                                                              \
    void test_##name()

#define CLICE_CHECK_IMPL(condition, return_action)                                                 \
    do {                                                                                           \
        if(condition) [[unlikely]] {                                                               \
            auto trace = cpptrace::generate_trace();                                               \
            ::eventide::zest::print_trace(trace, std::source_location::current());                           \
            failure();                                                                             \
            return_action;                                                                         \
        }                                                                                          \
    } while(0)

#define ZEST_EXPECT_UNARY(expr, expectation, failure_pred, return_action)                          \
    do {                                                                                           \
        auto _failed = ([&](auto&& _expr) {                                                        \
            return ::eventide::zest::check_unary_failure((failure_pred), #expr, (expectation), _expr);       \
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
            const auto _exprs = ::eventide::zest::parse_binary_exprs(#__VA_ARGS__);                          \
            return ::eventide::zest::check_binary_failure((failure_pred),                                    \
                                                #op_string,                                        \
                                                _exprs.lhs,                                        \
                                                _exprs.rhs,                                        \
                                                _lhs,                                              \
                                                _rhs);                                             \
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
#define ASSERT_NE(...) ZEST_EXPECT_BINARY(!=, ::eventide::zest::binary_equal(_lhs, _rhs), return, __VA_ARGS__)

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
            return ::eventide::zest::check_throws_failure((failure_pred), #expr, (expectation));             \
        }());                                                                                      \
        CLICE_CHECK_IMPL(_failed, return_action);                                                  \
    } while(0)

#define EXPECT_THROWS(expr) ZEST_EXPECT_THROWS(expr, "throw exception", !CAUGHT(expr), (void)0)
#define EXPECT_NOTHROWS(expr) ZEST_EXPECT_THROWS(expr, "not throw exception", CAUGHT(expr), (void)0)

#endif
