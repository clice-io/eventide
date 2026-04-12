#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace eventide::serde::schema {

// ---------------------------------------------------------------------------
// type_list -- compile-time type sequence
// ---------------------------------------------------------------------------

template <typename... Ts>
struct type_list {};

// -- element access --

template <std::size_t I, typename List>
struct type_list_element;

template <std::size_t I, typename First, typename... Rest>
struct type_list_element<I, type_list<First, Rest...>>
    : type_list_element<I - 1, type_list<Rest...>> {};

template <typename First, typename... Rest>
struct type_list_element<0, type_list<First, Rest...>> {
    using type = First;
};

template <std::size_t I, typename List>
using type_list_element_t = typename type_list_element<I, List>::type;

// -- concatenation (binary) --

template <typename A, typename B>
struct type_list_cat_impl;

template <typename... As, typename... Bs>
struct type_list_cat_impl<type_list<As...>, type_list<Bs...>> {
    using type = type_list<As..., Bs...>;
};

template <typename A, typename B>
using type_list_cat_t = typename type_list_cat_impl<A, B>::type;

// -- variadic concatenation --

template <typename... Lists>
struct type_list_concat;

template <>
struct type_list_concat<> {
    using type = type_list<>;
};

template <typename List>
struct type_list_concat<List> {
    using type = List;
};

template <typename First, typename Second, typename... Rest>
struct type_list_concat<First, Second, Rest...>
    : type_list_concat<type_list_cat_t<First, Second>, Rest...> {};

template <typename... Lists>
using type_list_concat_t = typename type_list_concat<Lists...>::type;

// -- size --

template <typename List>
struct type_list_size;

template <typename... Ts>
struct type_list_size<type_list<Ts...>>
    : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template <typename List>
inline constexpr std::size_t type_list_size_v = type_list_size<List>::value;

// ---------------------------------------------------------------------------
// field_slot -- compile-time per-field descriptor
// ---------------------------------------------------------------------------

/// Describes one logical (non-skipped, possibly-flattened) field at the type
/// level.  Carried inside a type_list so that virtual_schema can expose a
/// `slots` typedef for per-field compile-time dispatch.
///
/// @tparam RawType      The C++ field type (annotation unwrapped).
/// @tparam WireType     The type that actually hits the wire (after
///                       as<>/with<>/enum_string<> resolution).
///                       Defaults to RawType when no behavior attr is present.
/// @tparam BehaviorAttrs A std::tuple<...> of the behavior attributes found
///                       on this field (skip_if, with, as, enum_string, tagged).
template <typename RawType,
          typename WireType     = RawType,
          typename BehaviorAttrs = std::tuple<>>
struct field_slot {
    using raw_type  = RawType;
    using wire_type = WireType;
    using attrs     = BehaviorAttrs;
};

}  // namespace eventide::serde::schema
