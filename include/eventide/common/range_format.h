#pragma once

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

}  // namespace eventide
