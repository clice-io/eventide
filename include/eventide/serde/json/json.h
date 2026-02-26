#pragma once

#include <expected>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "eventide/reflection/struct.h"
#include "eventide/serde/json/simd_deserializer.h"
#include "eventide/serde/json/simd_serializer.h"

#if __has_include(<yyjson.h>)
#include "eventide/serde/json/dom.h"
#include "eventide/serde/json/yy_serializer.h"
#define EVENTIDE_SERDE_JSON_HAS_YYJSON 1
#else
#define EVENTIDE_SERDE_JSON_HAS_YYJSON 0
#endif

namespace eventide::serde::json {

namespace detail {

template <typename T>
consteval bool has_dynamic_dom_impl();

template <typename T>
consteval bool has_dynamic_dom_variant() {
    using U = std::remove_cvref_t<T>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (has_dynamic_dom_impl<std::variant_alternative_t<Is, U>>() || ...);
    }(std::make_index_sequence<std::variant_size_v<U>>{});
}

template <typename T>
consteval bool has_dynamic_dom_tuple_like() {
    using U = std::remove_cvref_t<T>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (has_dynamic_dom_impl<std::tuple_element_t<Is, U>>() || ...);
    }(std::make_index_sequence<std::tuple_size_v<U>>{});
}

template <typename T>
consteval bool has_dynamic_dom_reflectable() {
    using U = std::remove_cvref_t<T>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (has_dynamic_dom_impl<refl::field_type<U, Is>>() || ...);
    }(std::make_index_sequence<refl::field_count<U>()>{});
}

template <typename T>
consteval bool has_dynamic_dom_impl() {
    using U = std::remove_cvref_t<T>;

#if EVENTIDE_SERDE_JSON_HAS_YYJSON
    if constexpr(std::same_as<U, Value> || std::same_as<U, Array> || std::same_as<U, Object>) {
        return true;
    } else
#endif
        if constexpr(serde::annotated_type<U>) {
        return has_dynamic_dom_impl<typename U::annotated_type>();
    } else if constexpr(is_specialization_of<std::optional, U>) {
        return has_dynamic_dom_impl<typename U::value_type>();
    } else if constexpr(is_specialization_of<std::variant, U>) {
        return has_dynamic_dom_variant<U>();
    } else if constexpr(serde::is_pair_v<U> || serde::is_tuple_v<U>) {
        return has_dynamic_dom_tuple_like<U>();
    } else if constexpr(std::ranges::input_range<U>) {
        return has_dynamic_dom_impl<std::ranges::range_value_t<U>>();
    } else if constexpr(refl::reflectable_class<U>) {
        return has_dynamic_dom_reflectable<U>();
    } else {
        return false;
    }
}

}  // namespace detail

template <typename T>
constexpr inline bool has_dynamic_dom_v = detail::has_dynamic_dom_impl<T>();

template <typename T>
auto parse(std::string_view json, T& value) -> std::expected<void, simdjson::error_code> {
    if constexpr(!has_dynamic_dom_v<T>) {
        return simd::from_json(json, value);
    } else {
        return simd::from_json(json, value);
    }
}

template <typename T>
    requires std::default_initializable<T>
auto parse(std::string_view json) -> std::expected<T, simdjson::error_code> {
    if constexpr(!has_dynamic_dom_v<T>) {
        return simd::from_json<T>(json);
    } else {
        return simd::from_json<T>(json);
    }
}

template <typename T>
auto to_string(const T& value, std::optional<std::size_t> initial_capacity = std::nullopt)
    -> std::expected<std::string, simdjson::error_code> {
    if constexpr(!has_dynamic_dom_v<T>) {
        return simd::to_json(value, initial_capacity);
    } else {
#if EVENTIDE_SERDE_JSON_HAS_YYJSON
        (void)initial_capacity;
        auto json = yy::to_json(value);
        if(!json) {
            return std::unexpected(simdjson::TAPE_ERROR);
        }
        return std::move(*json);
#else
        (void)initial_capacity;
        return std::unexpected(simdjson::INCORRECT_TYPE);
#endif
    }
}

}  // namespace eventide::serde::json

#if EVENTIDE_SERDE_JSON_HAS_YYJSON
namespace eventide::serde {

namespace detail {

template <typename T>
concept json_dynamic_dom_type =
    std::same_as<T, json::Value> || std::same_as<T, json::Array> || std::same_as<T, json::Object>;

template <json_dynamic_dom_type T>
auto deserialize_dynamic_dom(json::simd::Deserializer& deserializer, T& value)
    -> std::expected<void, simdjson::error_code> {
    auto raw = deserializer.deserialize_raw_json_view();
    if(!raw) {
        return std::unexpected(raw.error());
    }

    auto parsed = T::parse(std::string_view(*raw));
    if(!parsed) {
        return std::unexpected(simdjson::TAPE_ERROR);
    }

    value = std::move(*parsed);
    return {};
}

template <json_dynamic_dom_type T>
auto serialize_dynamic_dom(json::simd::Serializer& serializer, const T& value)
    -> std::expected<typename json::simd::Serializer::value_type,
                     typename json::simd::Serializer::error_type> {
    auto raw = value.to_json_string();
    if(!raw) {
        return std::unexpected(simdjson::TAPE_ERROR);
    }
    return serializer.serialize_raw_json(*raw);
}

template <json_dynamic_dom_type T>
auto serialize_dynamic_dom(json::yy::Serializer& serializer, const T& value)
    -> std::expected<typename json::yy::Serializer::value_type,
                     typename json::yy::Serializer::error_type> {
    return serializer.append_json_value(value);
}

}  // namespace detail

template <>
struct deserialize_traits<json::simd::Deserializer, json::Value> {
    using error_type = simdjson::error_code;

    static auto deserialize(json::simd::Deserializer& deserializer, json::Value& value)
        -> std::expected<void, error_type> {
        return detail::deserialize_dynamic_dom(deserializer, value);
    }
};

template <>
struct deserialize_traits<json::simd::Deserializer, json::Array> {
    using error_type = simdjson::error_code;

    static auto deserialize(json::simd::Deserializer& deserializer, json::Array& value)
        -> std::expected<void, error_type> {
        return detail::deserialize_dynamic_dom(deserializer, value);
    }
};

template <>
struct deserialize_traits<json::simd::Deserializer, json::Object> {
    using error_type = simdjson::error_code;

    static auto deserialize(json::simd::Deserializer& deserializer, json::Object& value)
        -> std::expected<void, error_type> {
        return detail::deserialize_dynamic_dom(deserializer, value);
    }
};

template <>
struct serialize_traits<json::simd::Serializer, json::Value> {
    using value_type = typename json::simd::Serializer::value_type;
    using error_type = typename json::simd::Serializer::error_type;

    static auto serialize(json::simd::Serializer& serializer, const json::Value& value)
        -> std::expected<value_type, error_type> {
        return detail::serialize_dynamic_dom(serializer, value);
    }
};

template <>
struct serialize_traits<json::simd::Serializer, json::Array> {
    using value_type = typename json::simd::Serializer::value_type;
    using error_type = typename json::simd::Serializer::error_type;

    static auto serialize(json::simd::Serializer& serializer, const json::Array& value)
        -> std::expected<value_type, error_type> {
        return detail::serialize_dynamic_dom(serializer, value);
    }
};

template <>
struct serialize_traits<json::simd::Serializer, json::Object> {
    using value_type = typename json::simd::Serializer::value_type;
    using error_type = typename json::simd::Serializer::error_type;

    static auto serialize(json::simd::Serializer& serializer, const json::Object& value)
        -> std::expected<value_type, error_type> {
        return detail::serialize_dynamic_dom(serializer, value);
    }
};

template <>
struct serialize_traits<json::yy::Serializer, json::Value> {
    using value_type = typename json::yy::Serializer::value_type;
    using error_type = typename json::yy::Serializer::error_type;

    static auto serialize(json::yy::Serializer& serializer, const json::Value& value)
        -> std::expected<value_type, error_type> {
        return detail::serialize_dynamic_dom(serializer, value);
    }
};

template <>
struct serialize_traits<json::yy::Serializer, json::Array> {
    using value_type = typename json::yy::Serializer::value_type;
    using error_type = typename json::yy::Serializer::error_type;

    static auto serialize(json::yy::Serializer& serializer, const json::Array& value)
        -> std::expected<value_type, error_type> {
        return detail::serialize_dynamic_dom(serializer, value);
    }
};

template <>
struct serialize_traits<json::yy::Serializer, json::Object> {
    using value_type = typename json::yy::Serializer::value_type;
    using error_type = typename json::yy::Serializer::error_type;

    static auto serialize(json::yy::Serializer& serializer, const json::Object& value)
        -> std::expected<value_type, error_type> {
        return detail::serialize_dynamic_dom(serializer, value);
    }
};

}  // namespace eventide::serde
#endif
