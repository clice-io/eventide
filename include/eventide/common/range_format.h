#pragma once

#include <concepts>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "meta.h"

namespace eventide {

template <typename T>
constexpr bool is_map_value_v = false;

template <typename T, typename U>
constexpr bool is_map_value_v<std::pair<T, U>> = true;

template <typename T, typename U>
constexpr bool is_map_value_v<std::tuple<T, U>> = true;

template <typename T>
    requires std::is_reference_v<T> || std::is_const_v<T>
constexpr bool is_map_value_v<T> = is_map_value_v<std::remove_cvref_t<T>>;

enum class range_format { disabled, map, set, sequence };

template <class R>
constexpr range_format format_kind = [] {
    static_assert(dependent_false<R>, "instantiating a primary template is not allowed");
    return range_format::disabled;
}();

template <std::ranges::input_range R>
    requires std::same_as<R, std::remove_cvref_t<R>>
constexpr range_format format_kind<R> = [] {
    using ref_t = std::ranges::range_reference_t<R>;
    if constexpr(std::same_as<std::remove_cvref_t<ref_t>, R>) {
        return range_format::disabled;
    } else if constexpr(requires { typename R::key_type; }) {
        if constexpr(requires { typename R::mapped_type; } && is_map_value_v<ref_t>) {
            return range_format::map;
        } else {
            return range_format::set;
        }
    } else {
        return range_format::sequence;
    }
}();

template <typename T, range_format Kind>
concept range_of_kind = [] {
    using U = std::remove_cvref_t<T>;
    if constexpr(std::ranges::input_range<U>) {
        return format_kind<U> == Kind;
    } else {
        return false;
    }
}();

template <typename T>
concept sequence_range = range_of_kind<T, range_format::sequence>;

template <typename T>
concept set_range = range_of_kind<T, range_format::set>;

template <typename T>
concept map_range = range_of_kind<T, range_format::map>;

template <typename T>
concept ordered_associative_range =
    (set_range<T> || map_range<T>) && requires { typename T::key_compare; };

template <typename T>
concept unordered_associative_range = (set_range<T> || map_range<T>) && requires {
    typename T::hasher;
    typename T::key_equal;
};

template <typename T>
concept ordered_set_range = set_range<T> && ordered_associative_range<T>;

template <typename T>
concept ordered_map_range = map_range<T> && ordered_associative_range<T>;

template <typename T>
concept unordered_set_range = set_range<T> && unordered_associative_range<T>;

template <typename T>
concept unordered_map_range = map_range<T> && unordered_associative_range<T>;

}  // namespace eventide
