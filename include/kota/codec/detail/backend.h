#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "kota/support/ranges.h"
#include "kota/support/type_traits.h"
#include "kota/meta/type_kind.h"

namespace kota::codec {

enum class backend_kind { streaming, arena };

enum class field_mode { by_name, by_position, by_tag };

template <typename T>
concept null_like = meta::null_like<T>;

template <typename T>
concept bool_like = meta::bool_like<T>;

template <typename T>
concept int_like = meta::int_like<T>;

template <typename T>
concept uint_like = meta::uint_like<T>;

template <typename T>
concept floating_like = meta::floating_like<T>;

template <typename T>
concept char_like = meta::char_like<T>;

template <typename T>
concept str_like = meta::str_like<T>;

template <typename T>
concept bytes_like = meta::bytes_like<T>;

template <typename T>
constexpr inline bool is_pair_v = meta::is_pair_v<T>;

template <typename T>
constexpr inline bool is_tuple_v = meta::is_tuple_v<T>;

template <typename T>
concept tuple_like = meta::tuple_like<T>;

template <typename A, typename T, typename E>
concept result_as = std::same_as<A, std::expected<T, E>>;

template <typename E>
concept serde_error_like = requires {
    { E::type_mismatch } -> std::convertible_to<E>;
    { E::number_out_of_range } -> std::convertible_to<E>;
    { E::invalid_state } -> std::convertible_to<E>;
};

template <typename D, typename E = typename D::error_type>
concept deserializer_like =
    serde_error_like<E> && requires(D& d,
                                    bool& b,
                                    char& c,
                                    std::int64_t& i64,
                                    std::uint64_t& u64,
                                    double& f64,
                                    std::string& text,
                                    std::vector<std::byte>& bytes,
                                    std::variant<int, std::string>& variant_value) {
        { d.deserialize_bool(b) } -> result_as<void, E>;
        { d.deserialize_int(i64) } -> result_as<void, E>;
        { d.deserialize_uint(u64) } -> result_as<void, E>;
        { d.deserialize_float(f64) } -> result_as<void, E>;
        { d.deserialize_char(c) } -> result_as<void, E>;
        { d.deserialize_str(text) } -> result_as<void, E>;
        { d.deserialize_bytes(bytes) } -> result_as<void, E>;

        { d.deserialize_none() } -> result_as<bool, E>;
        { d.deserialize_variant(variant_value) } -> result_as<void, E>;

        // Streaming object interface
        { d.begin_object() } -> result_as<void, E>;
        { d.end_object() } -> result_as<void, E>;
        { d.next_field() } -> result_as<std::optional<std::string_view>, E>;
        { d.skip_field_value() } -> result_as<void, E>;

        // Streaming array interface
        { d.begin_array() } -> result_as<void, E>;
        { d.next_element() } -> result_as<bool, E>;
        { d.end_array() } -> result_as<void, E>;
    };

template <typename D, typename T>
struct deserialize_traits;

template <deserializer_like D, typename V, typename E = typename D::error_type>
constexpr auto deserialize(D& d, V& v) -> std::expected<void, E>;

}  // namespace kota::codec

namespace kota::codec::arena {

template <typename E>
concept arena_error_like = requires {
    { E::invalid_state } -> std::convertible_to<E>;
    { E::unsupported_type } -> std::convertible_to<E>;
    { E::type_mismatch } -> std::convertible_to<E>;
    { E::number_out_of_range } -> std::convertible_to<E>;
    { E::too_many_fields } -> std::convertible_to<E>;
};

template <typename D,
          typename E = typename D::error_type,
          typename TableView = typename D::TableView,
          typename SlotId = typename D::slot_id>
concept arena_deserializer_like = arena_error_like<E> && requires(D& d, std::size_t idx) {
    { D::field_slot_id(idx) } -> std::convertible_to<std::expected<SlotId, E>>;
    { D::variant_tag_slot_id() } -> std::convertible_to<SlotId>;
    { D::variant_payload_slot_id(idx) } -> codec::result_as<SlotId, E>;
    { d.root_view() } -> std::same_as<TableView>;
};

namespace detail {

template <typename D, typename T>
concept has_deserialize_wire_impl =
    requires { typename kota::codec::deserialize_traits<D, T>::wire_type; };

template <typename D, typename T>
concept value_deserialize_traits_impl =
    has_deserialize_wire_impl<D, T> &&
    requires(const D& d, typename kota::codec::deserialize_traits<D, T>::wire_type w) {
        {
            kota::codec::deserialize_traits<D, T>::deserialize(d, std::move(w))
        } -> std::convertible_to<T>;
    };

template <typename D, typename T>
concept streaming_deserialize_traits_impl =
    has_deserialize_wire_impl<D, T> &&
    requires(const D& d, typename D::TableView view, typename D::slot_id sid, T& out) {
        {
            kota::codec::deserialize_traits<D, T>::deserialize(d, view, sid, out)
        } -> std::same_as<std::expected<void, typename D::error_type>>;
    };

}  // namespace detail

template <typename D, typename T>
concept has_deserialize_traits = detail::has_deserialize_wire_impl<D, std::remove_cvref_t<T>>;

template <typename D, typename T>
concept value_deserialize_traits = detail::value_deserialize_traits_impl<D, std::remove_cvref_t<T>>;

template <typename D, typename T>
concept streaming_deserialize_traits =
    detail::streaming_deserialize_traits_impl<D, std::remove_cvref_t<T>>;

}  // namespace kota::codec::arena
