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

#include "eventide/common/meta.h"

namespace eventide::zest {

template <typename T>
inline std::string pretty_dump(const T& value) {
    if constexpr(is_optional_v<T>) {
        if(!value) {
            return std::string("nullopt");
        }
        return zest::pretty_dump(*value);
    } else if constexpr(is_expected_v<T>) {
        if(value.has_value()) {
            return std::format("expected({})", zest::pretty_dump(*value));
        }
        return std::format("unexpected({})", zest::pretty_dump(value.error()));
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
            static_assert(dependent_false<L>,
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
            static_assert(dependent_false<L>,
                          "EXPECT_EQ/ASSERT_EQ: lhs and expected value are not comparable");
            return false;
        }
    } else if constexpr(requires {
                            { lhs == rhs } -> std::convertible_to<bool>;
                        }) {
        return static_cast<bool>(lhs == rhs);
    } else {
        static_assert(dependent_false<L>, "EXPECT_EQ/ASSERT_EQ: operands are not comparable");
        return false;
    }
}

template <typename L, typename R>
inline bool binary_less(const L& lhs, const R& rhs) {
    if constexpr(requires {
                     { lhs < rhs } -> std::convertible_to<bool>;
                 }) {
        return static_cast<bool>(lhs < rhs);
    } else {
        static_assert(dependent_false<L>, "EXPECT_LT/ASSERT_LT: operands are not comparable");
        return false;
    }
}

template <typename L, typename R>
inline bool binary_less_equal(const L& lhs, const R& rhs) {
    if constexpr(requires {
                     { lhs <= rhs } -> std::convertible_to<bool>;
                 }) {
        return static_cast<bool>(lhs <= rhs);
    } else {
        static_assert(dependent_false<L>, "EXPECT_LE/ASSERT_LE: operands are not comparable");
        return false;
    }
}

template <typename L, typename R>
inline bool binary_greater(const L& lhs, const R& rhs) {
    if constexpr(requires {
                     { lhs > rhs } -> std::convertible_to<bool>;
                 }) {
        return static_cast<bool>(lhs > rhs);
    } else {
        static_assert(dependent_false<L>, "EXPECT_GT/ASSERT_GT: operands are not comparable");
        return false;
    }
}

template <typename L, typename R>
inline bool binary_greater_equal(const L& lhs, const R& rhs) {
    if constexpr(requires {
                     { lhs >= rhs } -> std::convertible_to<bool>;
                 }) {
        return static_cast<bool>(lhs >= rhs);
    } else {
        static_assert(dependent_false<L>, "EXPECT_GE/ASSERT_GE: operands are not comparable");
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
        std::println("           got: {}", zest::pretty_dump(value));
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
        std::println("           lhs: {}", zest::pretty_dump(lhs));
        std::println("           rhs: {}", zest::pretty_dump(rhs));
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
