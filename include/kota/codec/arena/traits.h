#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "kota/codec/detail/fwd.h"
#include "kota/codec/traits.h"

namespace kota::codec::arena {

// Arena-codec error contract. Extends the serde_error set with two error
// enumerators that only the arena/offset style backends need:
//
//   * unsupported_type — backend declined to encode/decode a particular type.
//   * too_many_fields   — slot id derived from field index overflowed the
//                         backend's slot-id representation.
//
// A backend's `error_type` enum just needs to name these six enumerators;
// the underlying storage is opaque to the trait.
template <typename E>
concept arena_error_like = requires {
    { E::invalid_state } -> std::convertible_to<E>;
    { E::unsupported_type } -> std::convertible_to<E>;
    { E::type_mismatch } -> std::convertible_to<E>;
    { E::number_out_of_range } -> std::convertible_to<E>;
    { E::too_many_fields } -> std::convertible_to<E>;
};

// Loose trait for arena-style serializers. Concrete backends expose:
//
//  * Offset/reference types: `string_ref`, `vector_ref`, `table_ref`.
//  * A `slot_id` handle identifying a slot inside an enclosing table.
//  * Helpers to derive slot ids from loop indices (fields, variant tag,
//    variant payload). Backends decide how to map a loop index to their
//    internal layout (e.g. flatbuffer's `4 + I*2`).
//  * Allocation entry points for strings, byte vectors, scalar/string/
//    inline-struct/table vectors. Scalar/inline-struct vectors are
//    templated — the concept only asserts the non-templated surface.
//  * A scoped `TableBuilder` returned by `start_table()` with `add_scalar`,
//    `add_offset`, `add_inline_struct` and `finalize()`. Templated members
//    are not enforced in the concept.
//  * `finish(root)` to seal the root table and `bytes()` to get the
//    serialized buffer.
template <typename S,
          typename E = typename S::error_type,
          typename StringRef = typename S::string_ref,
          typename VectorRef = typename S::vector_ref,
          typename TableRef = typename S::table_ref,
          typename SlotId = typename S::slot_id,
          typename TableBuilder = typename S::TableBuilder>
concept arena_serializer_like = arena_error_like<E> && requires(S& s,
                                                                std::string_view text,
                                                                std::span<const std::byte> bytes,
                                                                std::size_t idx,
                                                                TableRef root) {
    // slot-id helpers
    { S::field_slot_id(idx) } -> std::convertible_to<std::expected<SlotId, E>>;
    { S::variant_tag_slot_id() } -> std::convertible_to<SlotId>;
    { S::variant_payload_slot_id(idx) } -> std::convertible_to<std::expected<SlotId, E>>;

    // allocations
    { s.alloc_string(text) } -> result_as<StringRef, E>;
    { s.alloc_bytes(bytes) } -> result_as<VectorRef, E>;

    // table builder
    { s.start_table() } -> std::same_as<TableBuilder>;

    // finishing
    { s.finish(root) } -> result_as<void, E>;
    { s.bytes() } -> std::same_as<std::vector<std::uint8_t>>;
};

// Loose trait for arena-style deserializers.
//
//  * `TableView` wraps an opaque pointer to a read-only table and provides
//    `has(sid)` plus typed `get_*` accessors. Templated accessors
//    (`get_scalar<T>`, `get_inline_struct<T>`, `get_vector<T>`) are not
//    enforced in the concept.
//  * `VectorView` exposes size and indexed element access; it is obtained
//    from a `TableView` via `get_scalar_vector<T>(sid)` / analogous
//    templated accessors and is typed by the backend.
//  * `StringView` exposes string payload. Backends may alias this to a
//    `std::string_view`-like type.
template <typename D,
          typename E = typename D::error_type,
          typename TableView = typename D::TableView,
          typename SlotId = typename D::slot_id>
concept arena_deserializer_like = arena_error_like<E> && requires(D& d, std::size_t idx) {
    { D::field_slot_id(idx) } -> std::convertible_to<std::expected<SlotId, E>>;
    { D::variant_tag_slot_id() } -> std::convertible_to<SlotId>;
    { D::variant_payload_slot_id(idx) } -> std::convertible_to<std::expected<SlotId, E>>;
    { d.root_view() } -> std::same_as<TableView>;
};

/// Type-level serialization customization for the arena codec.
///
/// The arena codec reuses the primary `kota::codec::serialize_traits<S, V>`
/// / `deserialize_traits<D, V>` templates (declared in detail/fwd.h) — the
/// same customization point JSON / TOML / bincode already use. Arena
/// backends partially specialize these templates by constraining the
/// serializer / deserializer parameter with `arena_serializer_like<S>` /
/// `arena_deserializer_like<D>`:
///
///     template <typename S>
///         requires kota::codec::arena::arena_serializer_like<S>
///     struct kota::codec::serialize_traits<S, MyType> { ... };
///
///     template <typename D>
///         requires kota::codec::arena::arena_deserializer_like<D>
///     struct kota::codec::deserialize_traits<D, MyType> { ... };
///
/// Two modes are supported on each side:
///
/// * **Value mode** — the trait declares a `wire_type` and a pure value
///   transformation. The arena codec converts between `T` and `wire_type`
///   and then proceeds with normal dispatch on `wire_type` (so the wire
///   shape is exactly whatever encoding `wire_type` already has).
///
///     using wire_type = ...;
///     static wire_type    serialize(S&, const T&);
///     static T            deserialize(const D&, wire_type);
///
///   Use this for cheap leaf transformations — strong-typed ids ⇄
///   integers, enum-class wrappers ⇄ underlying, opaque byte blobs, etc.
///
/// * **Streaming mode** — the trait writes / reads directly through the
///   backend, avoiding any intermediate `wire_type` materialization.
///
///     using wire_type = ...;  // structural view type (used by proxy reads)
///     static auto serialize(S& s, const T& value)
///         -> std::expected<typename S::vector_ref /* or string_ref /
///                          bytes_ref / table_ref */, typename S::error_type>;
///
///     static auto deserialize(const D& d,
///                             typename D::TableView view,
///                             typename D::slot_id sid,
///                             T& out)
///         -> std::expected<void, typename D::error_type>;
///
///   Use this for heavy types where materializing an intermediate
///   `wire_type` value would be wasteful (e.g. an arena-interned string
///   pool streamed as a string vector — no transient `vector<string>`).
///
/// Both modes declare a `wire_type` typedef; the flatbuffers proxy uses it
/// to shape lazy views (e.g. `map_view<K, MyType>` exposes values as the
/// declared wire type).

namespace detail {

template <typename S, typename T>
concept has_serialize_wire_impl =
    requires { typename kota::codec::serialize_traits<S, T>::wire_type; };

template <typename D, typename T>
concept has_deserialize_wire_impl =
    requires { typename kota::codec::deserialize_traits<D, T>::wire_type; };

// Value-mode serialize: `static wire_type serialize(S&, const T&)` returning
// a value convertible to the declared wire_type.
template <typename S, typename T>
concept value_serialize_traits_impl =
    has_serialize_wire_impl<S, T> && requires(S& s, const T& v) {
        { kota::codec::serialize_traits<S, T>::serialize(s, v) }
            -> std::convertible_to<typename kota::codec::serialize_traits<S, T>::wire_type>;
    };

// Streaming serialize: `static expected<Ref, E> serialize(S&, const T&)`
// where Ref is one of the backend's offset handles. Distinguished from
// value-mode by a non-wire_type return — the `serialize` invocation must be
// well-formed but its result must NOT be convertible to the declared
// wire_type (otherwise value mode takes precedence).
template <typename S, typename T>
concept streaming_serialize_traits_impl =
    has_serialize_wire_impl<S, T> && !value_serialize_traits_impl<S, T> &&
    requires(S& s, const T& v) { kota::codec::serialize_traits<S, T>::serialize(s, v); };

// Value-mode deserialize: `static T deserialize(const D&, wire_type)`.
template <typename D, typename T>
concept value_deserialize_traits_impl =
    has_deserialize_wire_impl<D, T> &&
    requires(const D& d, typename kota::codec::deserialize_traits<D, T>::wire_type w) {
        { kota::codec::deserialize_traits<D, T>::deserialize(d, std::move(w)) }
            -> std::convertible_to<T>;
    };

// Streaming deserialize: `static expected<void, E> deserialize(const D&,
// TableView, slot_id, T&)`. The trait reads the value straight off the
// view / slot, handling missing-field semantics itself.
template <typename D, typename T>
concept streaming_deserialize_traits_impl =
    has_deserialize_wire_impl<D, T> && requires(const D& d,
                                                typename D::TableView view,
                                                typename D::slot_id sid,
                                                T& out) {
        {
            kota::codec::deserialize_traits<D, T>::deserialize(d, view, sid, out)
        } -> std::same_as<std::expected<void, typename D::error_type>>;
    };

}  // namespace detail

/// True when `kota::codec::serialize_traits<S, T>` exposes *any* arena
/// specialization (declares a `wire_type`). Used by the proxy / arena
/// codec to decide whether to substitute `T` with its wire type during
/// dispatch.
template <typename S, typename T>
concept has_serialize_traits = detail::has_serialize_wire_impl<S, std::remove_cvref_t<T>>;

template <typename D, typename T>
concept has_deserialize_traits = detail::has_deserialize_wire_impl<D, std::remove_cvref_t<T>>;

/// True when the specialization exposes a value-mode
/// `serialize(S&, const T&) -> wire_type`.
template <typename S, typename T>
concept value_serialize_traits = detail::value_serialize_traits_impl<S, std::remove_cvref_t<T>>;

/// True when the specialization exposes a streaming-mode
/// `serialize(S&, const T&) -> expected<Ref, E>`.
template <typename S, typename T>
concept streaming_serialize_traits =
    detail::streaming_serialize_traits_impl<S, std::remove_cvref_t<T>>;

/// True when the specialization exposes a value-mode
/// `deserialize(D&, wire_type) -> T`.
template <typename D, typename T>
concept value_deserialize_traits = detail::value_deserialize_traits_impl<D, std::remove_cvref_t<T>>;

/// True when the specialization exposes a streaming-mode
/// `deserialize(const D&, TableView, slot_id, T&) -> expected<void, E>`.
template <typename D, typename T>
concept streaming_deserialize_traits =
    detail::streaming_deserialize_traits_impl<D, std::remove_cvref_t<T>>;

}  // namespace kota::codec::arena
