#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include "eventide/reflection/struct.h"
#include "eventide/serde/serde/annotation.h"
#include "eventide/serde/serde/attrs/schema.h"
#include "eventide/serde/serde/config.h"
#include "eventide/serde/serde/schema/kind.h"
#include "eventide/serde/serde/spelling.h"

namespace eventide::serde::schema {

struct field_descriptor {
    std::string_view name;
    type_kind kind;
    bool required;
};

template <std::size_t N>
struct type_descriptor {
    std::array<field_descriptor, N> fields{};

    constexpr std::size_t field_count() const {
        return N;
    }
};

namespace detail {

template <typename T>
consteval auto unwrap_annotation_type() {
    using U = std::remove_cvref_t<T>;
    if constexpr(serde::annotated_type<U>) {
        return std::type_identity<typename U::annotated_type>{};
    } else {
        return std::type_identity<U>{};
    }
}

template <typename T>
using unwrapped_t = typename decltype(unwrap_annotation_type<T>())::type;

template <typename T, std::size_t I>
consteval bool field_has_flatten() {
    using field_t = refl::field_type<T, I>;
    if constexpr(!serde::has_attrs<field_t>) {
        return false;
    } else {
        return serde::detail::tuple_has_v<typename field_t::attrs, serde::schema::flatten>;
    }
}

template <typename T, std::size_t I>
consteval bool field_has_skip() {
    using field_t = refl::field_type<T, I>;
    if constexpr(!serde::has_attrs<field_t>) {
        return false;
    } else {
        return serde::detail::tuple_has_v<typename field_t::attrs, serde::schema::skip>;
    }
}

template <typename T, std::size_t I>
consteval bool field_has_explicit_rename() {
    using field_t = refl::field_type<T, I>;
    if constexpr(!serde::has_attrs<field_t>) {
        return false;
    } else {
        return serde::detail::tuple_any_of_v<typename field_t::attrs, serde::is_rename_attr>;
    }
}

template <typename T>
consteval std::size_t effective_field_count();

template <typename T, std::size_t I>
consteval std::size_t single_field_contribution() {
    if constexpr(field_has_skip<T, I>()) {
        return 0;
    } else if constexpr(field_has_flatten<T, I>()) {
        using field_t = refl::field_type<T, I>;
        using inner_t = unwrapped_t<field_t>;
        static_assert(refl::reflectable_class<inner_t>,
                      "schema::flatten requires a reflectable class");
        return effective_field_count<inner_t>();
    } else {
        return 1;
    }
}

template <typename T>
consteval std::size_t effective_field_count() {
    if constexpr(!refl::reflectable_class<std::remove_cvref_t<T>>) {
        return 0;
    } else {
        using U = std::remove_cvref_t<T>;
        constexpr auto N = refl::field_count<U>();
        if constexpr(N == 0) {
            return 0;
        } else {
            return []<std::size_t... Is>(std::index_sequence<Is...>) consteval {
                return (single_field_contribution<U, Is>() + ...);
            }(std::make_index_sequence<N>{});
        }
    }
}

template <typename T, std::size_t I>
consteval std::string_view resolve_field_name() {
    return serde::schema::canonical_field_name<T, I>();
}

template <typename T, std::size_t I>
consteval type_kind resolve_field_kind() {
    using field_t = refl::field_type<T, I>;
    using value_t = unwrapped_t<field_t>;
    if constexpr(is_optional_v<value_t>) {
        return kind_of<typename value_t::value_type>();
    } else {
        return kind_of<value_t>();
    }
}

template <typename T, std::size_t I>
consteval bool resolve_field_required() {
    using field_t = refl::field_type<T, I>;
    return !is_optional_type<field_t>();
}

template <typename T, std::size_t N>
consteval void fill_fields(std::array<field_descriptor, N>& arr, std::size_t& idx);

template <typename T, std::size_t I, std::size_t N>
consteval void fill_one_field(std::array<field_descriptor, N>& arr, std::size_t& idx) {
    if constexpr(field_has_skip<T, I>()) {
        // skip: contribute nothing
    } else if constexpr(field_has_flatten<T, I>()) {
        using field_t = refl::field_type<T, I>;
        using inner_t = unwrapped_t<field_t>;
        fill_fields<inner_t>(arr, idx);
    } else {
        arr[idx++] = field_descriptor{
            .name = resolve_field_name<T, I>(),
            .kind = resolve_field_kind<T, I>(),
            .required = resolve_field_required<T, I>(),
        };
    }
}

template <typename T, std::size_t N>
consteval void fill_fields(std::array<field_descriptor, N>& arr, std::size_t& idx) {
    using U = std::remove_cvref_t<T>;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) consteval {
        (fill_one_field<U, Is>(arr, idx), ...);
    }(std::make_index_sequence<refl::field_count<U>()>{});
}

}  // namespace detail

/// Generate a compile-time schema for reflectable type T.
/// Flatten fields are recursively expanded.
/// Skip fields are omitted.
/// Field names use canonical_field_name (handles schema::rename attrs).
template <typename T>
    requires refl::reflectable_class<std::remove_cvref_t<T>>
consteval auto describe() {
    constexpr auto N = detail::effective_field_count<T>();
    type_descriptor<N> desc{};
    if constexpr(N > 0) {
        std::size_t idx = 0;
        detail::fill_fields<T>(desc.fields, idx);
    }
    return desc;
}

/// Convenience: static schema for type T.
template <typename T>
    requires refl::reflectable_class<std::remove_cvref_t<T>>
struct schema_of {
    static constexpr auto value = describe<T>();
};

}  // namespace eventide::serde::schema
