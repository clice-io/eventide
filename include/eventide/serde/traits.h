#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace eventide::serde {

template <typename T>
constexpr inline bool dependent_false = false;

template <typename T, typename... Ts>
concept if_one_of = (std::same_as<T, Ts> || ...);

template <template <typename...> typename HKT, typename T>
constexpr inline bool is_specialization_of = false;

template <template <typename...> typename HKT, typename... Ts>
constexpr inline bool is_specialization_of<HKT, HKT<Ts...>> = true;

template <typename T>
concept bool_like = std::same_as<T, bool>;

template <typename T>
concept int_like = if_one_of<T,
                             signed char,
                             short,
                             int,
                             long,
                             long long,
                             std::int8_t,
                             std::int16_t,
                             std::int32_t,
                             std::int64_t>;

template <typename T>
concept uint_like = if_one_of<T,
                              unsigned char,
                              unsigned short,
                              unsigned int,
                              unsigned long,
                              unsigned long long,
                              std::uint8_t,
                              std::uint16_t,
                              std::uint32_t,
                              std::uint64_t>;

template <typename T>
concept floating_like = if_one_of<T, float, double, long double>;

template <typename T>
concept char_like = std::same_as<T, char>;

template <typename T>
concept str_like = std::convertible_to<T, std::string_view>;

template <typename T>
concept bytes_like = std::convertible_to<T, std::span<const std::byte>>;

template <typename T>
constexpr inline bool is_pair_v = is_specialization_of<std::pair, T>;

template <typename T>
constexpr inline bool is_tuple_v = is_specialization_of<std::tuple, T>;

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

template <typename A, typename T, typename E>
concept result_as = std::same_as<A, std::expected<T, E>>;

template <typename S,
          typename T = typename S::value_type,
          typename E = typename S::error_type,
          typename SerializeSeq = typename S::SerializeSeq,
          typename SerializeTuple = typename S::SerializeTuple,
          typename SerializeMap = typename S::SerializeMap,
          typename SerializeStruct = typename S::SerializeStruct>
concept serializer_like = requires(S& s,
                                   bool b,
                                   char c,
                                   std::int64_t i,
                                   std::uint64_t u,
                                   double f,
                                   std::string_view text,
                                   std::span<const std::byte> bytes,
                                   std::optional<std::size_t> len,
                                   std::size_t tuple_len,
                                   const int& key,
                                   const int& value) {
    { s.serialize_none() } -> result_as<T, E>;
    { s.serialize_some(i) } -> result_as<T, E>;

    { s.serialize_bool(b) } -> result_as<T, E>;
    { s.serialize_int(i) } -> result_as<T, E>;
    { s.serialize_uint(u) } -> result_as<T, E>;
    { s.serialize_float(f) } -> result_as<T, E>;
    { s.serialize_char(c) } -> result_as<T, E>;
    { s.serialize_str(text) } -> result_as<T, E>;
    { s.serialize_bytes(bytes) } -> result_as<T, E>;

    { s.serialize_seq(len) } -> result_as<SerializeSeq, E>;
    requires requires(SerializeSeq& s) {
        { s.serialize_element(value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };

    { s.serialize_tuple(tuple_len) } -> result_as<SerializeTuple, E>;
    requires requires(SerializeTuple& s) {
        { s.serialize_element(value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };

    { s.serialize_map(len) } -> result_as<SerializeMap, E>;
    requires requires(SerializeMap& s) {
        { s.serialize_entry(key, value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };

    { s.serialize_struct(text, tuple_len) } -> result_as<SerializeStruct, E>;
    requires requires(SerializeStruct& s) {
        { s.serialize_field(text, value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };
};

template <typename D,
          typename E = typename D::error_type,
          typename DeserializeSeq = typename D::DeserializeSeq,
          typename DeserializeTuple = typename D::DeserializeTuple,
          typename DeserializeMap = typename D::DeserializeMap,
          typename DeserializeStruct = typename D::DeserializeStruct>
concept deserializer_like = requires(D& d,
                                     bool& b,
                                     char& c,
                                     std::int64_t& i64,
                                     std::uint64_t& u64,
                                     double& f64,
                                     std::string& text,
                                     std::vector<std::byte>& bytes,
                                     std::optional<std::size_t> len,
                                     std::size_t tuple_len,
                                     std::string_view name,
                                     int& value) {
    { d.deserialize_none() } -> result_as<bool, E>;
    { d.deserialize_some(value) } -> result_as<void, E>;

    { d.deserialize_bool(b) } -> result_as<void, E>;
    { d.deserialize_int(i64) } -> result_as<void, E>;
    { d.deserialize_uint(u64) } -> result_as<void, E>;
    { d.deserialize_float(f64) } -> result_as<void, E>;
    { d.deserialize_char(c) } -> result_as<void, E>;
    { d.deserialize_str(text) } -> result_as<void, E>;
    { d.deserialize_bytes(bytes) } -> result_as<void, E>;

    { d.deserialize_seq(len) } -> result_as<DeserializeSeq, E>;
    requires requires(DeserializeSeq& s) {
        { s.has_next() } -> result_as<bool, E>;
        { s.deserialize_element(value) } -> result_as<void, E>;
        { s.skip_element() } -> result_as<void, E>;
        { s.end() } -> result_as<void, E>;
    };

    { d.deserialize_tuple(tuple_len) } -> result_as<DeserializeTuple, E>;
    requires requires(DeserializeTuple& s) {
        { s.deserialize_element(value) } -> result_as<void, E>;
        { s.skip_element() } -> result_as<void, E>;
        { s.end() } -> result_as<void, E>;
    };

    { d.deserialize_map(len) } -> result_as<DeserializeMap, E>;
    requires requires(DeserializeMap& s) {
        { s.next_key() } -> result_as<std::optional<std::string_view>, E>;
        { s.deserialize_value(value) } -> result_as<void, E>;
        { s.skip_value() } -> result_as<void, E>;
        { s.end() } -> result_as<void, E>;
    };

    { d.deserialize_struct(name, tuple_len) } -> result_as<DeserializeStruct, E>;
    requires requires(DeserializeStruct& s) {
        { s.next_key() } -> result_as<std::optional<std::string_view>, E>;
        { s.deserialize_value(value) } -> result_as<void, E>;
        { s.skip_value() } -> result_as<void, E>;
        { s.end() } -> result_as<void, E>;
    };
};

}  // namespace eventide::serde
