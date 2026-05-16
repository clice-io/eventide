#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

#include "kota/codec/detail/codec.h"
#include "kota/codec/detail/common.h"

#if __has_include(<flatbuffers/flatbuffers.h>)
#include "flatbuffers/flatbuffers.h"
#else
#error "flatbuffers/flatbuffers.h not found."
#endif

namespace kota::codec::fbs {

// Builder
using builder_t = ::flatbuffers::FlatBufferBuilder;

// Slot/offset primitives
using voffset_t = ::flatbuffers::voffset_t;
using uoffset_t = ::flatbuffers::uoffset_t;

// Typed offsets
template <typename T>
using offset_t = ::flatbuffers::Offset<T>;
using table_offset_t = offset_t<::flatbuffers::Table>;
using string_offset_t = offset_t<::flatbuffers::String>;

// FlatBuffer wire types
using fb_table = ::flatbuffers::Table;
using fb_string = ::flatbuffers::String;
template <typename T>
using fb_vector = ::flatbuffers::Vector<T>;

// Verification
using verifier_t = ::flatbuffers::Verifier;

enum class object_error_code : std::uint8_t {
    none = 0,
    invalid_state,
    unsupported_type,
    type_mismatch,
    number_out_of_range,
    too_many_fields,
};

constexpr std::string_view error_message(object_error_code code) {
    switch(code) {
        case object_error_code::none: return "none";
        case object_error_code::invalid_state: return "invalid state";
        case object_error_code::unsupported_type: return "unsupported type";
        case object_error_code::type_mismatch: return "type mismatch";
        case object_error_code::number_out_of_range: return "number out of range";
        case object_error_code::too_many_fields: return "too many fields";
    }
    return "invalid state";
}

template <typename T>
using object_result_t = std::expected<T, object_error_code>;

namespace detail {

constexpr inline char buffer_identifier[] = "EVTO";
constexpr voffset_t first_field = 4;
constexpr voffset_t field_step = 2;

inline auto field_voffset(std::size_t index) -> object_result_t<voffset_t> {
    constexpr auto max_voffset = static_cast<std::size_t>((std::numeric_limits<voffset_t>::max)());
    const auto raw =
        static_cast<std::size_t>(first_field) + index * static_cast<std::size_t>(field_step);
    if(raw > max_voffset) {
        return std::unexpected(object_error_code::too_many_fields);
    }
    return static_cast<voffset_t>(raw);
}

inline auto variant_payload_voffset(std::size_t index) -> object_result_t<voffset_t> {
    return field_voffset(index + 1);
}

}  // namespace detail

namespace schema_detail {

using codec::detail::remove_annotation_t;
using codec::detail::remove_optional_t;
using codec::detail::clean_t;

template <typename T>
constexpr bool is_scalar_field_v =
    std::same_as<T, bool> || codec::int_like<T> || codec::uint_like<T> || codec::floating_like<T>;

template <typename T>
struct schema_struct_trait;

template <typename T>
constexpr bool is_schema_struct_field_v = [] {
    using U = remove_optional_t<T>;
    if constexpr(is_scalar_field_v<U> || std::is_enum_v<U>) {
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
