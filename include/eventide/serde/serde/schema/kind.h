#pragma once

#include <cstdint>
#include <optional>
#include <ranges>
#include <type_traits>
#include <variant>

#include "eventide/common/meta.h"
#include "eventide/common/ranges.h"
#include "eventide/reflection/struct.h"
#include "eventide/serde/serde/annotation.h"
#include "eventide/serde/serde/traits.h"

namespace eventide::serde::schema {

enum class type_kind : std::uint8_t {
    null_like = 0,
    boolean = 1,
    integer = 2,
    floating = 3,
    string = 4,
    array = 5,
    object = 6,
    variant = 7,
    any = 255,
};

template <typename T>
consteval type_kind kind_of() {
    using U = std::remove_cvref_t<T>;

    if constexpr(serde::annotated_type<U>) {
        return kind_of<typename U::annotated_type>();
    } else if constexpr(is_specialization_of<std::optional, U>) {
        return kind_of<typename U::value_type>();
    } else if constexpr(serde::null_like<U>) {
        return type_kind::null_like;
    } else if constexpr(serde::bool_like<U>) {
        return type_kind::boolean;
    } else if constexpr(serde::int_like<U> || serde::uint_like<U> || std::is_enum_v<U>) {
        return type_kind::integer;
    } else if constexpr(serde::floating_like<U>) {
        return type_kind::floating;
    } else if constexpr(serde::char_like<U> || serde::str_like<U>) {
        return type_kind::string;
    } else if constexpr(is_specialization_of<std::variant, U>) {
        return type_kind::variant;
    } else if constexpr(serde::bytes_like<U>) {
        return type_kind::array;
    } else if constexpr(serde::is_pair_v<U> || serde::is_tuple_v<U>) {
        return type_kind::array;
    } else if constexpr(std::ranges::input_range<U>) {
        constexpr auto fmt = format_kind<U>;
        if constexpr(fmt == range_format::map) {
            return type_kind::object;
        } else {
            return type_kind::array;
        }
    } else if constexpr(refl::reflectable_class<U>) {
        return type_kind::object;
    } else {
        return type_kind::any;
    }
}

template <typename T>
consteval bool is_optional_type() {
    using U = std::remove_cvref_t<T>;
    if constexpr(serde::annotated_type<U>) {
        return is_optional_type<typename U::annotated_type>();
    } else {
        return is_optional_v<U>;
    }
}

/// Convert a serde::type_hint bitmask (from peek_type_hint) to a schema::type_kind.
/// Picks the most specific single kind; returns type_kind::any on ambiguity.
constexpr type_kind hint_to_kind(std::uint8_t hint_bits) {
    if(hint_bits & 0x40) return type_kind::object;
    if(hint_bits & 0x20) return type_kind::array;
    if(hint_bits & 0x10) return type_kind::string;
    if(hint_bits & 0x08) return type_kind::floating;
    if(hint_bits & 0x04) return type_kind::integer;
    if(hint_bits & 0x02) return type_kind::boolean;
    if(hint_bits & 0x01) return type_kind::null_like;
    return type_kind::any;
}

}  // namespace eventide::serde::schema
