#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "eventide/error.h"
#include "serde/enum.h"

namespace eventide::serde {

template <typename T>
using result = eventide::result<T>;

template <class Deserializer>
using deserialize_error_t = typename decltype(std::declval<Deserializer&>().root())::error_type;

template <class T, class Deserializer>
using deserialize_result_t = std::expected<T, deserialize_error_t<Deserializer>>;

template <class Reader>
using value_type_t = typename Reader::value_type;

template <class Reader>
using object_type_t = typename Reader::object_type;

template <class Reader>
using array_type_t = typename Reader::array_type;

template <class T>
struct serialize_traits {};

template <class T>
struct deserialize_traits {};

namespace detail {

template <class...>
constexpr inline bool always_false_v = false;

template <class Error>
struct error_traits;

template <>
struct error_traits<eventide::error> {
    constexpr static eventide::error invalid_argument() {
        return eventide::error::invalid_argument;
    }

    constexpr static eventide::error no_data_available() {
        return eventide::error::no_data_available;
    }
};

template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <class T>
constexpr inline bool is_string_v = std::is_same_v<remove_cvref_t<T>, std::string> ||
                                    std::is_same_v<remove_cvref_t<T>, std::string_view>;

template <class T>
struct is_optional : std::false_type {};

template <class T>
struct is_optional<std::optional<T>> : std::true_type {};
template <class T>
constexpr inline bool is_optional_v = is_optional<remove_cvref_t<T>>::value;

template <class T>
struct is_vector : std::false_type {};

template <class T, class A>
struct is_vector<std::vector<T, A>> : std::true_type {};
template <class T>
constexpr inline bool is_vector_v = is_vector<remove_cvref_t<T>>::value;

template <class T>
struct is_map : std::false_type {};

template <class K, class V, class C, class A>
struct is_map<std::map<K, V, C, A>> : std::true_type {};
template <class T>
constexpr inline bool is_map_v = is_map<remove_cvref_t<T>>::value;

template <class T>
struct is_variant : std::false_type {};

template <class... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};
template <class T>
constexpr inline bool is_variant_v = is_variant<remove_cvref_t<T>>::value;

template <class T>
struct is_tuple : std::false_type {};

template <class... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};
template <class T>
constexpr inline bool is_tuple_v = is_tuple<remove_cvref_t<T>>::value;

template <class K>
std::string key_to_string(const K& key) {
    if constexpr(std::is_same_v<remove_cvref_t<K>, std::string>) {
        return key;
    } else if constexpr(std::is_same_v<remove_cvref_t<K>, std::string_view>) {
        return std::string(key);
    } else if constexpr(std::is_integral_v<remove_cvref_t<K>>) {
        return std::to_string(key);
    } else {
        static_assert(always_false_v<K>, "Map key type must be string-like or integral.");
    }
}

template <class K>
bool parse_key(std::string_view text, K& out) {
    if constexpr(std::is_same_v<remove_cvref_t<K>, std::string>) {
        out = std::string(text);
        return true;
    } else if constexpr(std::is_same_v<remove_cvref_t<K>, std::string_view>) {
        out = text;
        return true;
    } else if constexpr(std::is_integral_v<remove_cvref_t<K>>) {
        std::string tmp(text);
        if(tmp.empty()) {
            return false;
        }
        char* end = nullptr;
        if constexpr(std::is_signed_v<remove_cvref_t<K>>) {
            long long value = std::strtoll(tmp.c_str(), &end, 10);
            if(end == tmp.c_str() || *end != '\0') {
                return false;
            }
            out = static_cast<remove_cvref_t<K>>(value);
        } else {
            unsigned long long value = std::strtoull(tmp.c_str(), &end, 10);
            if(end == tmp.c_str() || *end != '\0') {
                return false;
            }
            out = static_cast<remove_cvref_t<K>>(value);
        }
        return true;
    } else {
        return false;
    }
}

template <class T, class Serializer>
concept has_serialize_traits = requires(const T& value, Serializer& serializer) {
    { serialize_traits<T>::serialize(value, serializer) } -> std::same_as<void>;
};

template <class T, class Serializer>
concept has_member_serialize = requires(const T& value, Serializer& serializer) {
    { value.serialize(serializer) } -> std::same_as<void>;
};

template <class T, class Deserializer>
concept has_deserialize_traits =
    requires(Deserializer& deserializer, value_type_t<Deserializer> value) {
        {
            deserialize_traits<T>::deserialize(deserializer, value)
        } -> std::same_as<deserialize_result_t<T, Deserializer>>;
    };

template <class T, class Deserializer>
concept has_static_deserialize =
    requires(Deserializer& deserializer, value_type_t<Deserializer> value) {
        {
            T::deserialize(deserializer, value)
        } -> std::same_as<deserialize_result_t<T, Deserializer>>;
    };

}  // namespace detail

template <class Serializer, class T>
void serialize(Serializer& serializer, const T& value) {
    using value_t = detail::remove_cvref_t<T>;

    if constexpr(detail::has_serialize_traits<value_t, Serializer>) {
        serialize_traits<value_t>::serialize(value, serializer);
    } else if constexpr(detail::has_member_serialize<value_t, Serializer>) {
        value.serialize(serializer);
    } else if constexpr(std::is_same_v<value_t, const char*> || std::is_same_v<value_t, char*>) {
        if(value == nullptr) {
            serializer.serialize_none();
        } else {
            serializer.serialize_str(std::string_view(value));
        }
    } else if constexpr(std::is_array_v<value_t> &&
                        (std::is_same_v<std::remove_extent_t<value_t>, const char> ||
                         std::is_same_v<std::remove_extent_t<value_t>, char>)) {
        serializer.serialize_str(std::string_view(value));
    } else if constexpr(std::is_same_v<value_t, bool>) {
        serializer.serialize_bool(value);
    } else if constexpr(std::is_same_v<value_t, char>) {
        serializer.serialize_char(value);
    } else if constexpr(std::is_integral_v<value_t>) {
        if constexpr(std::is_signed_v<value_t>) {
            serializer.serialize_i(value);
        } else {
            serializer.serialize_u(value);
        }
    } else if constexpr(std::is_floating_point_v<value_t>) {
        serializer.serialize_f(value);
    } else if constexpr(detail::is_string_v<value_t>) {
        serializer.serialize_str(std::string_view(value));
    } else if constexpr(detail::is_optional_v<value_t>) {
        if(value) {
            serializer.serialize_some(*value);
        } else {
            serializer.serialize_none();
        }
    } else if constexpr(detail::is_vector_v<value_t>) {
        std::optional<std::size_t> len = static_cast<std::size_t>(value.size());
        auto seq = serializer.serialize_seq(len);
        for(const auto& item: value) {
            seq.serialize_element(item);
        }
        seq.end();
    } else if constexpr(detail::is_map_v<value_t>) {
        std::optional<std::size_t> len = static_cast<std::size_t>(value.size());
        auto map = serializer.serialize_map(len);
        for(const auto& [key, item]: value) {
            map.serialize_entry(key, item);
        }
        map.end();
    } else if constexpr(detail::is_variant_v<value_t>) {
        std::visit([&](const auto& item) { serialize(serializer, item); }, value);
    } else if constexpr(detail::is_tuple_v<value_t>) {
        auto tuple = serializer.serialize_tuple(std::tuple_size_v<value_t>);
        std::apply([&](const auto&... items) { (tuple.serialize_element(items), ...); }, value);
        tuple.end();
    } else if constexpr(refl::serde::has_enum_traits<value_t>) {
        if constexpr(refl::serde::enum_traits<value_t>::encoding ==
                     refl::serde::enum_encoding::integer) {
            serializer.serialize_i(static_cast<std::underlying_type_t<value_t>>(value));
        } else {
            serializer.serialize_str(refl::serde::enum_to_string(value));
        }
    } else if constexpr(std::is_enum_v<value_t>) {
        serializer.serialize_i(static_cast<std::underlying_type_t<value_t>>(value));
    } else {
        static_assert(
            detail::always_false_v<T>,
            "No Serialize implementation for this type. " "Provide serialize_traits<T>::serialize or member serialize().");
    }
}

template <class T, class Deserializer>
deserialize_result_t<T, Deserializer> deserialize(Deserializer& deserializer,
                                                  value_type_t<Deserializer> value) {
    using value_t = detail::remove_cvref_t<T>;
    using error_t = deserialize_error_t<Deserializer>;

    if constexpr(detail::has_deserialize_traits<value_t, Deserializer>) {
        return deserialize_traits<value_t>::deserialize(deserializer, value);
    } else if constexpr(detail::has_static_deserialize<value_t, Deserializer>) {
        return value_t::deserialize(deserializer, value);
    } else if constexpr(detail::is_optional_v<value_t>) {
        if(deserializer.is_null(value)) {
            return std::optional<typename value_t::value_type>{};
        }
        auto inner = deserialize<typename value_t::value_type>(deserializer, value);
        if(!inner) {
            return std::unexpected(inner.error());
        }
        return std::optional<typename value_t::value_type>(std::move(*inner));
    } else if constexpr(detail::is_vector_v<value_t>) {
        auto array = deserializer.as_array(value);
        if(!array) {
            return std::unexpected(array.error());
        }
        value_t out{};
        for(auto item: *array) {
            auto parsed = deserialize<typename value_t::value_type>(deserializer, item);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            out.push_back(std::move(*parsed));
        }
        return out;
    } else if constexpr(detail::is_map_v<value_t>) {
        auto object = deserializer.as_object(value);
        if(!object) {
            return std::unexpected(object.error());
        }
        value_t out{};
        for(auto field: *object) {
            typename value_t::key_type parsed_key{};
            if(!detail::parse_key(field.key, parsed_key)) {
                return std::unexpected(detail::error_traits<error_t>::invalid_argument());
            }
            auto parsed_value =
                deserialize<typename value_t::mapped_type>(deserializer, field.value);
            if(!parsed_value) {
                return std::unexpected(parsed_value.error());
            }
            out.emplace(std::move(parsed_key), std::move(*parsed_value));
        }
        return out;
    } else if constexpr(detail::is_variant_v<value_t>) {
        deserialize_result_t<value_t, Deserializer> out =
            std::unexpected(detail::error_traits<error_t>::invalid_argument());
        bool matched = false;

        auto try_one = [&](auto index_const) {
            if(matched) {
                return;
            }
            using alt_t = std::variant_alternative_t<decltype(index_const)::value, value_t>;
            auto parsed = deserialize<alt_t>(deserializer, value);
            if(parsed) {
                out = value_t{std::in_place_type<alt_t>, std::move(*parsed)};
                matched = true;
            }
        };

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (try_one(std::integral_constant<std::size_t, Is>{}), ...);
        }(std::make_index_sequence<std::variant_size_v<value_t>>{});

        return out;
    } else if constexpr(detail::is_tuple_v<value_t>) {
        auto array = deserializer.as_array(value);
        if(!array) {
            return std::unexpected(array.error());
        }

        value_t out{};
        std::size_t index = 0;
        for(auto item: *array) {
            bool assigned = false;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((index == Is ? (void)([&] {
                     using elem_t = std::tuple_element_t<Is, value_t>;
                     auto parsed = deserialize<elem_t>(deserializer, item);
                     if(parsed) {
                         std::get<Is>(out) = std::move(*parsed);
                         assigned = true;
                     }
                 }())
                              : void()),
                 ...);
            }(std::make_index_sequence<std::tuple_size_v<value_t>>{});

            ++index;
            if(!assigned) {
                return std::unexpected(detail::error_traits<error_t>::invalid_argument());
            }
        }
        return out;
    } else if constexpr(refl::serde::has_enum_traits<value_t>) {
        if constexpr(refl::serde::enum_traits<value_t>::encoding ==
                     refl::serde::enum_encoding::integer) {
            auto parsed = deserializer.as_i64(value);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            return static_cast<value_t>(*parsed);
        } else {
            auto text = deserializer.as_string(value);
            if(!text) {
                return std::unexpected(text.error());
            }
            value_t out{};
            if(!refl::serde::enum_from_string(*text, out)) {
                return std::unexpected(detail::error_traits<error_t>::invalid_argument());
            }
            return out;
        }
    } else if constexpr(std::is_enum_v<value_t>) {
        auto parsed = deserializer.as_i64(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        return static_cast<value_t>(*parsed);
    } else if constexpr(detail::is_string_v<value_t>) {
        auto text = deserializer.as_string(value);
        if(!text) {
            return std::unexpected(text.error());
        }
        if constexpr(std::is_same_v<value_t, std::string>) {
            return std::string(*text);
        } else {
            return *text;
        }
    } else if constexpr(std::is_same_v<value_t, char>) {
        auto text = deserializer.as_string(value);
        if(!text || text->size() != 1) {
            return std::unexpected(detail::error_traits<error_t>::invalid_argument());
        }
        return (*text)[0];
    } else if constexpr(std::is_same_v<value_t, bool>) {
        return deserializer.as_bool(value);
    } else if constexpr(std::is_integral_v<value_t>) {
        if constexpr(std::is_signed_v<value_t>) {
            auto parsed = deserializer.as_i64(value);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            return static_cast<value_t>(*parsed);
        } else {
            auto parsed = deserializer.as_u64(value);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            return static_cast<value_t>(*parsed);
        }
    } else if constexpr(std::is_floating_point_v<value_t>) {
        auto parsed = deserializer.as_f64(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        return static_cast<value_t>(*parsed);
    } else {
        static_assert(
            detail::always_false_v<T>,
            "No Deserialize implementation for this type. " "Provide deserialize_traits<T>::deserialize or static deserialize().");
    }
}

template <class T, class Deserializer>
deserialize_result_t<T, Deserializer> deserialize(Deserializer& deserializer) {
    auto value = deserializer.root();
    if(!value) {
        return std::unexpected(value.error());
    }
    return deserialize<T>(deserializer, *value);
}

}  // namespace eventide::serde
