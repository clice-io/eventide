#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

#include "kota/meta/repr.h"
#include "kota/meta/type_kind.h"
#include "kota/codec/visit/context.h"

#if __has_include(<flatbuffers/flatbuffers.h>)
#include "flatbuffers/flatbuffers.h"
#else
#error "flatbuffers/flatbuffers.h not found."
#endif

namespace kota::codec::fbs {

using builder_t = ::flatbuffers::FlatBufferBuilder;

using voffset_t = ::flatbuffers::voffset_t;
using uoffset_t = ::flatbuffers::uoffset_t;

using Table = ::flatbuffers::Table;
using String = ::flatbuffers::String;
template <typename T>
using Vector = ::flatbuffers::Vector<T>;

template <typename T>
using offset_t = ::flatbuffers::Offset<T>;
using table_offset_t = offset_t<Table>;
using string_offset_t = offset_t<String>;

using verifier_t = ::flatbuffers::Verifier;

/// Format tag: scopes a meta::repr specialization to the flatbuffers backend
/// (meta::repr<T, codec::fbs::format>).
///
/// # Lowerings
///
/// FlatBuffers binary built in two passes (allocate child offsets, then
/// write tables); layout_computed=true, so meta::dynamic reprs are rejected
/// at compile time. Field slots use voffsets first_field=4, field_step=2 in
/// declaration order; the buffer carries the "EVTO" identifier.
/// - root → always a table: structures map directly, any other kind is
///   boxed into a single-field table at the first slot, and a null root is
///   an empty table
/// - boolean → uint8 cell; character → int8 cell; long double → double
///   cell; other scalars → their natural fixed-width cells
/// - string → flatbuffers string; bytes → vector of uint8
/// - enumeration → underlying integer cell
/// - structure → table with one slot per field; structs satisfying
///   can_inline_struct_v (trivial, standard-layout, unannotated scalar
///   fields) may instead inline as fixed-size structs inside vectors
/// - array/set → vector; element storage follows element_layout (proxy.h):
///   scalar cells, strings, inline structs, tables, or boxed tables for
///   nullable / variant-shaped elements
/// - tuple → table with one slot per element
/// - map → sorted vector of two-field {key, value} entry tables; the
///   ordering key mirrors find_entry (strings lexicographic, enums by
///   underlying value, scalars by value) so lookups can binary-search
/// - variant → table with the u32 alternative index at the first slot and
///   the payload at the slot for that alternative (variant_payload_voffset)
/// - optional/pointer → disengaged fields simply leave their slot absent;
///   as vector elements they degrade to boxed tables
struct format {};

enum class object_error_code : std::uint8_t {
    None = 0,
    TooManyFields,
};

template <typename T>
using object_result_t = std::expected<T, object_error_code>;

namespace detail {

/// Static visitor traits shared by the fbs visitors the codec dispatch
/// drives: a binary backend whose output layout is computed statically (so
/// meta::dynamic reprs are rejected at compile time).
struct VisitorBase {
    using error_type = rich_error;
    using format = fbs::format;
    constexpr static bool human_readable = false;
    constexpr static bool layout_computed = true;
};

constexpr inline char buffer_identifier[] = "EVTO";
constexpr voffset_t first_field = 4;
constexpr voffset_t field_step = 2;

inline auto field_voffset(std::size_t index) -> object_result_t<voffset_t> {
    constexpr auto max_voffset = static_cast<std::size_t>((std::numeric_limits<voffset_t>::max)());
    const auto raw =
        static_cast<std::size_t>(first_field) + index * static_cast<std::size_t>(field_step);
    if(raw > max_voffset) {
        return std::unexpected(object_error_code::TooManyFields);
    }
    return static_cast<voffset_t>(raw);
}

inline auto variant_payload_voffset(std::size_t index) -> object_result_t<voffset_t> {
    return field_voffset(index + 1);
}

}  // namespace detail

namespace schema_detail {

template <typename T>
struct remove_optional {
    using type = T;
};

template <typename T>
struct remove_optional<std::optional<T>> {
    using type = T;
};

/// meta::field_type yields const-qualified types, so strip cv before matching.
template <typename T>
using remove_optional_t = typename remove_optional<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_scalar_field_v =
    std::same_as<T, bool> || meta::int_like<T> || meta::uint_like<T> || meta::floating_like<T> ||
    meta::char_like<T> || std::same_as<T, std::byte>;

template <typename T>
struct schema_struct_trait;

template <typename T>
constexpr bool is_schema_struct_field_v = [] {
    using U = remove_optional_t<T>;
    if constexpr(meta::has_repr<U, format>) {
        // A repr replaces the raw layout, so the field cannot be part of an
        // inline struct's memcpy image; the enclosing type degrades to a
        // table, where the dispatch applies the repr.
        return false;
    } else if constexpr(is_scalar_field_v<U> || std::is_enum_v<U>) {
        return true;
    } else if constexpr(meta::reflectable_class<U>) {
        return schema_struct_trait<U>::value;
    } else {
        return false;
    }
}();

template <typename T>
struct schema_struct_trait {
    static consteval bool fields_supported() {
        if constexpr(!meta::reflectable_class<T>) {
            return false;
        } else {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (is_schema_struct_field_v<meta::field_type<T, I>> && ...);
            }(std::make_index_sequence<meta::field_count<T>()>{});
        }
    }

    constexpr static bool value = meta::reflectable_class<T> && std::is_trivial_v<T> &&
                                  std::is_standard_layout_v<T> && fields_supported();
};

template <typename T>
constexpr bool is_schema_struct_v = schema_struct_trait<T>::value;

}  // namespace schema_detail

template <typename T>
constexpr bool is_schema_struct_v = schema_detail::is_schema_struct_v<T>;

template <typename T>
consteval bool has_annotated_fields() {
    using U = std::remove_cvref_t<T>;
    if constexpr(!meta::reflectable_class<U>) {
        return false;
    } else {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return (meta::annotated_type<meta::field_type<U, I>> || ...);
        }(std::make_index_sequence<meta::field_count<U>()>{});
    }
}

template <typename T>
constexpr bool can_inline_struct_v =
    meta::reflectable_class<T> && is_schema_struct_v<T> && !has_annotated_fields<T>();

}  // namespace kota::codec::fbs
