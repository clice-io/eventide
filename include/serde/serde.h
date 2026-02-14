#pragma once

#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "reflection/struct.h"
#include "serde/attrs.h"
#include "serde/concepts.h"
#include "serde/enum.h"
#include "serde/traits.h"

namespace eventide::serde {

template <typename T, typename... Ts>
concept same_as_any = (std::same_as<T, Ts> || ...);

template <typename T>
concept bool_like = std::same_as<T, bool>;

template <typename T>
concept int_like = same_as_any<T,
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
concept uint_like = same_as_any<T,
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
concept floating_like = same_as_any<T, float, double, long double>;

template <typename T>
concept char_like = std::same_as<T, char>;

template <typename T>
concept str_like = std::convertible_to<T, std::string_view>;

template <typename T>
concept bytes_like = std::convertible_to<T, std::span<const std::byte>>;

enum class range_format { disabled, map, set, sequence };

template <typename>
constexpr bool is_pair_or_tuple_2 = false;

template <typename T, typename U>
constexpr bool is_pair_or_tuple_2<std::pair<T, U>> = true;

template <typename T, typename U>
constexpr bool is_pair_or_tuple_2<std::tuple<T, U>> = true;

template <typename T>
    requires std::is_reference_v<T> || std::is_const_v<T>
constexpr bool is_pair_or_tuple_2<T> = is_pair_or_tuple_2<std::remove_cvref_t<T>>;

template <typename>
constexpr bool dependent_false_v = false;

template <class R>
constexpr range_format format_kind = [] {
    static_assert(dependent_false_v<R>, "instantiating a primary template is not allowed");
    return range_format::disabled;
}();

template <std::ranges::input_range R>
    requires std::same_as<R, std::remove_cvref_t<R>>
constexpr range_format format_kind<R> = [] {
    if constexpr(std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, R>) {
        return range_format::disabled;
    } else if constexpr(requires { typename R::key_type; }) {
        if constexpr(requires { typename R::mapped_type; } &&
                     is_pair_or_tuple_2<std::ranges::range_reference_t<R>>) {
            return range_format::map;
        } else {
            return range_format::set;
        }
    } else {
        return range_format::sequence;
    }
}();

template <typename D, typename T>
struct serialize_traits;

template <serializer_like S, typename V>
auto serialize(S& s, const V& v) {
    using Serde = serialize_traits<S, V>;
    if constexpr(requires { Serde::serialize(s, v); }) {
        return Serde::serialize(s, v);
    } else if constexpr(bool_like<V>) {
        return s.serialize_bool(v);
    } else if constexpr(int_like<V>) {
        return s.serialize_int(v);
    } else if constexpr(uint_like<V>) {
        return s.serialize_uint(v);
    } else if constexpr(char_like<V>) {
        return s.serialize_char(v);
    } else if constexpr(str_like<V>) {
        return s.serialize_str(v);
    } else if constexpr(bytes_like<V>) {
        return s.serialize_bytes(v);
    } else if constexpr(std::ranges::input_range<V>) {
        constexpr auto kind = format_kind<V>;
        if constexpr(kind == range_format::sequence) {
            auto seq = s.serialize_seq(std::ranges::size(v));
            for(auto& e: v) {
                seq.serialize_element(e);
            }
            seq.end();
        } else if constexpr(kind == range_format::map) {
            auto map = s.serialize_map(std::ranges::size(v));
            for(auto& [key, value]: v) {
                map.serialize_entry(key, value);
            }
            map.end();
        }
    }
}

template <typename R, typename Deserializer>
auto deserialize(Deserializer& s) {}

}  // namespace eventide::serde
