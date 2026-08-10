#pragma once

#include "type_kind.h"
#include "kota/support/type_traits.h"

namespace kota::meta {

/// Declares the wire representation of a type: how T travels on every backend
/// and how it appears to every schema consumer (JSON Schema export,
/// flatbuffers layout, zero-copy views). Specialize it for your own types;
/// the primary template is intentionally undefined.
///
/// Declarative form (preferred — works on all backends, full schema support):
///
///     template <>
///     struct kota::meta::repr<RelationKind> {
///         using type = std::uint32_t;
///         static type to(RelationKind k);    // encode: T -> type
///         static RelationKind from(type v);  // decode: type -> T
///     };
///
/// Either direction may be omitted; using the missing direction is a compile
/// error. Signatures are checked against `type` at the call site.
///
/// Imperative form (streaming backends only; the body drives the visitor):
///
///     template <>
///     struct kota::meta::repr<Weird> {
///         using type = std::string;  // still required: schema stays honest
///         template <typename Config>
///         static bool serialize(auto& vis, const Weird& w);
///         template <typename Config>
///         static bool deserialize(auto& vis, Weird& w);
///     };
///
/// Use `using type = meta::dynamic;` when the shape is only known at runtime:
/// schema output degrades to "any", and layout-computed backends
/// (flatbuffers) reject the type at compile time.
///
/// Per-field customization (behavior::with / behavior::as annotations) takes
/// priority over the field type's repr.
template <typename T>
struct repr;

/// True when repr<T> is specialized (the primary template is never defined).
template <typename T>
concept has_repr = requires { sizeof(repr<T>); };

namespace detail {

template <typename Shape>
constexpr auto wire_shape_impl() {
    if constexpr(requires { typename Shape::type; }) {
        return std::type_identity<typename Shape::type>{};
    } else {
        static_assert(dependent_false<Shape>,
                      "a meta::repr specialization or behavior::with adapter must declare its "
                      "wire shape via `using type = ...` (meta::dynamic when only known at "
                      "runtime)");
        return std::type_identity<void>{};
    }
}

}  // namespace detail

/// The declared wire shape of a repr specialization or with-adapter; a missing
/// `type` member is a hard error with the protocol diagnostic.
template <typename Shape>
using wire_shape_t = typename decltype(detail::wire_shape_impl<Shape>())::type;

namespace detail {

template <typename T>
constexpr auto resolved_repr_impl() {
    if constexpr(has_repr<T>) {
        return resolved_repr_impl<wire_shape_t<repr<T>>>();
    } else {
        return std::type_identity<T>{};
    }
}

}  // namespace detail

/// The final wire shape of a value type: chained reprs (a repr whose declared
/// wire type itself has a repr) are followed to the repr-free end, matching
/// the codec dispatch, which re-enters encode/decode on the converted wire
/// value. Identity when T declares no repr.
template <typename T>
using resolved_repr_t = typename decltype(detail::resolved_repr_impl<T>())::type;

}  // namespace kota::meta
