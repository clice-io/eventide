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
using deserialize_error_t = typename Deserializer::error_type;

template <class T, class Deserializer>
using deserialize_result_t = std::expected<T, deserialize_error_t<Deserializer>>;

template <class Reader>
using value_type_t = typename Reader::value_type;

template <class Reader>
using object_type_t = typename Reader::object_type;

template <class Reader>
using array_type_t = typename Reader::array_type;

template <class Serializer, class T>
struct serialize_traits {};

template <class Deserializer, class T>
struct deserialize_traits {};

struct ignored_any {};

template <class T, class Deserializer>
deserialize_result_t<T, Deserializer> deserialize(Deserializer& deserializer);

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

template <class T>
struct is_expected : std::false_type {};

template <class T, class E>
struct is_expected<std::expected<T, E>> : std::true_type {};
template <class T>
constexpr inline bool is_expected_v = is_expected<remove_cvref_t<T>>::value;

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

template <class Serializer, class T>
concept has_serialize_traits_for = requires(const T& value, Serializer& serializer) {
    serialize_traits<Serializer, T>::serialize(serializer, value);
};

template <class Serializer>
concept serializer = requires(Serializer& serializer,
                              bool b,
                              char c,
                              std::int64_t i,
                              std::uint64_t u,
                              double f,
                              std::string_view text,
                              std::optional<std::size_t> len,
                              std::size_t tuple_len,
                              const int& key,
                              const int& value) {
    { serializer.serialize_bool(b) } -> std::same_as<void>;
    { serializer.serialize_i(i) } -> std::same_as<void>;
    { serializer.serialize_u(u) } -> std::same_as<void>;
    { serializer.serialize_f(f) } -> std::same_as<void>;
    { serializer.serialize_char(c) } -> std::same_as<void>;
    { serializer.serialize_str(text) } -> std::same_as<void>;
    { serializer.serialize_none() } -> std::same_as<void>;
    { serializer.serialize_some(value) } -> std::same_as<void>;
    { serializer.serialize_unit() } -> std::same_as<void>;

    { serializer.serialize_seq(len).serialize_element(value) } -> std::same_as<void>;
    { serializer.serialize_seq(len).end() } -> std::same_as<void>;

    { serializer.serialize_tuple(tuple_len).serialize_element(value) } -> std::same_as<void>;
    { serializer.serialize_tuple(tuple_len).end() } -> std::same_as<void>;

    { serializer.serialize_map(len).serialize_entry(key, value) } -> std::same_as<void>;
    { serializer.serialize_map(len).end() } -> std::same_as<void>;

    {
        serializer.serialize_struct(text, tuple_len).serialize_field(text, value)
    } -> std::same_as<void>;
    { serializer.serialize_struct(text, tuple_len).end() } -> std::same_as<void>;

    { serializer.is_human_readable() } -> std::convertible_to<bool>;
};

template <class T, class Serializer>
concept has_serialize_traits = requires(const T& value, Serializer& serializer) {
    requires has_serialize_traits_for<Serializer, T>;
};

template <class T, class Serializer>
concept has_member_serialize =
    requires(const T& value, Serializer& serializer) { value.serialize(serializer); };

template <class Serializer>
void serialize_signed(Serializer& serializer, std::int64_t value) {
    if constexpr(requires(Serializer& s, std::int64_t i) { s.serialize_int(i); }) {
        (void)serializer.serialize_int(value);
    } else {
        (void)serializer.serialize_i(value);
    }
}

template <class Serializer>
void serialize_unsigned(Serializer& serializer, std::uint64_t value) {
    if constexpr(requires(Serializer& s, std::uint64_t u) { s.serialize_uint(u); }) {
        (void)serializer.serialize_uint(value);
    } else {
        (void)serializer.serialize_u(value);
    }
}

template <class Serializer>
void serialize_floating(Serializer& serializer, double value) {
    if constexpr(requires(Serializer& s, double f) { s.serialize_float(f); }) {
        (void)serializer.serialize_float(value);
    } else {
        (void)serializer.serialize_f(value);
    }
}

template <class Access>
bool access_ready(const Access& access) {
    if constexpr(is_expected_v<Access>) {
        return access.has_value();
    } else {
        return true;
    }
}

template <class Access>
decltype(auto) access_ref(Access& access) {
    if constexpr(is_expected_v<Access>) {
        return *access;
    } else {
        return (access);
    }
}

template <class Deserializer>
struct bool_visitor {
    using value_type = bool;

    std::string_view expecting() const {
        return "boolean";
    }

    deserialize_result_t<bool, Deserializer> visit_bool(bool value) {
        return value;
    }
};

template <class Deserializer>
struct i64_visitor {
    using value_type = std::int64_t;

    std::string_view expecting() const {
        return "integer";
    }

    deserialize_result_t<std::int64_t, Deserializer> visit_i64(std::int64_t value) {
        return value;
    }
};

template <class Deserializer>
struct u64_visitor {
    using value_type = std::uint64_t;

    std::string_view expecting() const {
        return "unsigned integer";
    }

    deserialize_result_t<std::uint64_t, Deserializer> visit_u64(std::uint64_t value) {
        return value;
    }
};

template <class Deserializer>
struct f64_visitor {
    using value_type = double;

    std::string_view expecting() const {
        return "floating point";
    }

    deserialize_result_t<double, Deserializer> visit_f64(double value) {
        return value;
    }
};

template <class Deserializer>
struct string_visitor {
    using value_type = std::string;

    std::string_view expecting() const {
        return "string";
    }

    deserialize_result_t<std::string, Deserializer> visit_string(std::string value) {
        return value;
    }

    deserialize_result_t<std::string, Deserializer> visit_str(std::string_view value) {
        return std::string(value);
    }
};

template <class Deserializer>
struct string_view_visitor {
    using value_type = std::string_view;

    std::string_view expecting() const {
        return "string";
    }

    deserialize_result_t<std::string_view, Deserializer>
        visit_borrowed_str(std::string_view value) {
        return value;
    }

    deserialize_result_t<std::string_view, Deserializer> visit_str(std::string_view value) {
        return value;
    }
};

template <class Deserializer>
struct char_visitor {
    using value_type = char;

    std::string_view expecting() const {
        return "character";
    }

    deserialize_result_t<char, Deserializer> visit_char(char value) {
        return value;
    }
};

template <class Deserializer, class Inner>
struct option_visitor {
    using value_type = std::optional<Inner>;

    std::string_view expecting() const {
        return "optional value";
    }

    deserialize_result_t<value_type, Deserializer> visit_none() {
        return value_type{};
    }

    deserialize_result_t<value_type, Deserializer> visit_unit() {
        return value_type{};
    }

    deserialize_result_t<value_type, Deserializer> visit_some(Deserializer& deserializer) {
        auto inner = eventide::serde::deserialize<Inner>(deserializer);
        if(!inner) {
            return std::unexpected(inner.error());
        }
        return value_type(std::move(*inner));
    }
};

template <class Deserializer, class Vector>
struct vector_visitor {
    using value_type = Vector;
    using element_type = typename Vector::value_type;

    std::string_view expecting() const {
        return "sequence";
    }

    template <class Access>
    deserialize_result_t<Vector, Deserializer> visit_seq(Access& access) {
        Vector out{};
        if constexpr(requires(Vector& v, std::size_t n) { v.reserve(n); }) {
            auto hint = access.size_hint();
            if(hint) {
                out.reserve(*hint);
            }
        }
        while(true) {
            auto parsed = access.template next_element<element_type>();
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            if(!*parsed) {
                break;
            }
            out.push_back(std::move(**parsed));
        }
        return out;
    }
};

template <class Deserializer, class Map>
struct map_visitor {
    using value_type = Map;
    using key_type = typename Map::key_type;
    using mapped_type = typename Map::mapped_type;

    std::string_view expecting() const {
        return "map";
    }

    template <class Access>
    deserialize_result_t<Map, Deserializer> visit_map(Access& access) {
        Map out{};
        while(true) {
            auto key = access.template next_key<key_type>();
            if(!key) {
                return std::unexpected(key.error());
            }
            if(!*key) {
                break;
            }
            auto value = access.template next_value<mapped_type>();
            if(!value) {
                return std::unexpected(value.error());
            }
            out.emplace(std::move(**key), std::move(*value));
        }
        return out;
    }
};

template <class Deserializer, class Tuple>
struct tuple_visitor {
    using value_type = Tuple;
    using error_t = deserialize_error_t<Deserializer>;

    std::string_view expecting() const {
        return "tuple";
    }

    template <class Access>
    deserialize_result_t<Tuple, Deserializer> visit_seq(Access& access) {
        Tuple out{};
        std::optional<error_t> tuple_error;
        bool ended_early = false;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (([&] {
                 if(tuple_error || ended_early) {
                     return;
                 }
                 using elem_t = std::tuple_element_t<Is, Tuple>;
                 auto parsed = access.template next_element<elem_t>();
                 if(!parsed) {
                     tuple_error = parsed.error();
                     return;
                 }
                 if(!*parsed) {
                     ended_early = true;
                     return;
                 }
                 std::get<Is>(out) = std::move(**parsed);
             }()),
             ...);
        }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});

        if(tuple_error) {
            return std::unexpected(*tuple_error);
        }

        if(!ended_early) {
            auto extra = access.template next_element<ignored_any>();
            if(!extra) {
                return std::unexpected(extra.error());
            }
            if(*extra) {
                return std::unexpected(error_traits<error_t>::invalid_argument());
            }
        }

        return out;
    }
};

template <class Deserializer>
struct ignored_any_visitor {
    using value_type = ignored_any;

    std::string_view expecting() const {
        return "ignored value";
    }

    deserialize_result_t<ignored_any, Deserializer> visit_ignored_any() {
        return ignored_any{};
    }

    deserialize_result_t<ignored_any, Deserializer> visit_unit() {
        return ignored_any{};
    }
};

template <class Deserializer, class T>
concept has_deserialize_traits_for = requires(Deserializer& deserializer) {
    {
        deserialize_traits<Deserializer, T>::deserialize(deserializer)
    } -> std::same_as<deserialize_result_t<T, Deserializer>>;
};

template <class T, class Deserializer>
concept has_deserialize_traits =
    requires(Deserializer& deserializer) { requires has_deserialize_traits_for<Deserializer, T>; };

template <class T, class Deserializer>
concept has_static_deserialize = requires(Deserializer& deserializer) {
    { T::deserialize(deserializer) } -> std::same_as<deserialize_result_t<T, Deserializer>>;
};

}  // namespace detail

template <class Serializer, class T>
void serialize(Serializer& serializer, const T& value) {
    using value_t = detail::remove_cvref_t<T>;

    if constexpr(detail::has_serialize_traits_for<Serializer, value_t>) {
        serialize_traits<Serializer, value_t>::serialize(serializer, value);
    } else if constexpr(detail::has_member_serialize<value_t, Serializer>) {
        value.serialize(serializer);
    } else if constexpr(std::is_same_v<value_t, const char*> || std::is_same_v<value_t, char*>) {
        if(value == nullptr) {
            (void)serializer.serialize_none();
        } else {
            (void)serializer.serialize_str(std::string_view(value));
        }
    } else if constexpr(std::is_array_v<value_t> &&
                        (std::is_same_v<std::remove_extent_t<value_t>, const char> ||
                         std::is_same_v<std::remove_extent_t<value_t>, char>)) {
        (void)serializer.serialize_str(std::string_view(value));
    } else if constexpr(std::is_same_v<value_t, bool>) {
        (void)serializer.serialize_bool(value);
    } else if constexpr(std::is_same_v<value_t, char>) {
        (void)serializer.serialize_char(value);
    } else if constexpr(std::is_integral_v<value_t>) {
        if constexpr(std::is_signed_v<value_t>) {
            detail::serialize_signed(serializer, static_cast<std::int64_t>(value));
        } else {
            detail::serialize_unsigned(serializer, static_cast<std::uint64_t>(value));
        }
    } else if constexpr(std::is_floating_point_v<value_t>) {
        detail::serialize_floating(serializer, static_cast<double>(value));
    } else if constexpr(detail::is_string_v<value_t>) {
        (void)serializer.serialize_str(std::string_view(value));
    } else if constexpr(detail::is_optional_v<value_t>) {
        if(value) {
            serialize(serializer, *value);
        } else {
            (void)serializer.serialize_none();
        }
    } else if constexpr(detail::is_vector_v<value_t>) {
        std::optional<std::size_t> len = static_cast<std::size_t>(value.size());
        auto seq_access = serializer.serialize_seq(len);
        if(!detail::access_ready(seq_access)) {
            return;
        }
        auto& seq = detail::access_ref(seq_access);
        for(const auto& item: value) {
            (void)seq.serialize_element(item);
        }
        (void)seq.end();
    } else if constexpr(detail::is_map_v<value_t>) {
        std::optional<std::size_t> len = static_cast<std::size_t>(value.size());
        auto map_access = serializer.serialize_map(len);
        if(!detail::access_ready(map_access)) {
            return;
        }
        auto& map = detail::access_ref(map_access);
        for(const auto& [key, item]: value) {
            (void)map.serialize_entry(key, item);
        }
        (void)map.end();
    } else if constexpr(detail::is_variant_v<value_t>) {
        std::visit([&](const auto& item) { serialize(serializer, item); }, value);
    } else if constexpr(detail::is_tuple_v<value_t>) {
        auto tuple_access = serializer.serialize_tuple(std::tuple_size_v<value_t>);
        if(!detail::access_ready(tuple_access)) {
            return;
        }
        auto& tuple = detail::access_ref(tuple_access);
        std::apply([&](const auto&... items) { ((void)tuple.serialize_element(items), ...); },
                   value);
        (void)tuple.end();
    } else if constexpr(refl::serde::has_enum_traits<value_t>) {
        if constexpr(refl::serde::enum_traits<value_t>::encoding ==
                     refl::serde::enum_encoding::integer) {
            using underlying_t = std::underlying_type_t<value_t>;
            if constexpr(std::is_signed_v<underlying_t>) {
                detail::serialize_signed(serializer, static_cast<std::int64_t>(value));
            } else {
                detail::serialize_unsigned(serializer, static_cast<std::uint64_t>(value));
            }
        } else {
            (void)serializer.serialize_str(refl::serde::enum_to_string(value));
        }
    } else if constexpr(std::is_enum_v<value_t>) {
        using underlying_t = std::underlying_type_t<value_t>;
        if constexpr(std::is_signed_v<underlying_t>) {
            detail::serialize_signed(serializer, static_cast<std::int64_t>(value));
        } else {
            detail::serialize_unsigned(serializer, static_cast<std::uint64_t>(value));
        }
    } else {
        static_assert(
            detail::always_false_v<T>,
            "No Serialize implementation for this type. " "Provide serialize_traits<T>::serialize or member serialize().");
    }
}

template <class T, class Deserializer>
deserialize_result_t<T, Deserializer> deserialize(Deserializer& deserializer) {
    using value_t = detail::remove_cvref_t<T>;
    using error_t = deserialize_error_t<Deserializer>;

    if constexpr(detail::has_deserialize_traits_for<Deserializer, value_t>) {
        return deserialize_traits<Deserializer, value_t>::deserialize(deserializer);
    } else if constexpr(detail::has_static_deserialize<value_t, Deserializer>) {
        return value_t::deserialize(deserializer);
    } else if constexpr(std::is_same_v<value_t, ignored_any>) {
        detail::ignored_any_visitor<Deserializer> visitor{};
        return deserializer.deserialize_ignored_any(visitor);
    } else if constexpr(detail::is_optional_v<value_t>) {
        using inner_t = typename value_t::value_type;
        detail::option_visitor<Deserializer, inner_t> visitor{};
        return deserializer.deserialize_option(visitor);
    } else if constexpr(detail::is_vector_v<value_t>) {
        detail::vector_visitor<Deserializer, value_t> visitor{};
        return deserializer.deserialize_seq(visitor);
    } else if constexpr(detail::is_map_v<value_t>) {
        detail::map_visitor<Deserializer, value_t> visitor{};
        return deserializer.deserialize_map(visitor);
    } else if constexpr(detail::is_variant_v<value_t>) {
        deserialize_result_t<value_t, Deserializer> out =
            std::unexpected(detail::error_traits<error_t>::invalid_argument());
        bool matched = false;

        auto try_one = [&](auto index_const) {
            if(matched) {
                return;
            }
            using alt_t = std::variant_alternative_t<decltype(index_const)::value, value_t>;
            auto parsed = deserialize<alt_t>(deserializer);
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
        detail::tuple_visitor<Deserializer, value_t> visitor{};
        return deserializer.deserialize_tuple(std::tuple_size_v<value_t>, visitor);
    } else if constexpr(refl::serde::has_enum_traits<value_t>) {
        if constexpr(refl::serde::enum_traits<value_t>::encoding ==
                     refl::serde::enum_encoding::integer) {
            detail::i64_visitor<Deserializer> visitor{};
            auto parsed = deserializer.deserialize_i(visitor);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            return static_cast<value_t>(*parsed);
        } else {
            detail::string_view_visitor<Deserializer> visitor{};
            auto text = deserializer.deserialize_str(visitor);
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
        detail::i64_visitor<Deserializer> visitor{};
        auto parsed = deserializer.deserialize_i(visitor);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        return static_cast<value_t>(*parsed);
    } else if constexpr(std::is_same_v<value_t, std::string>) {
        detail::string_visitor<Deserializer> visitor{};
        auto text = deserializer.deserialize_string(visitor);
        if(!text) {
            return std::unexpected(text.error());
        }
        return std::move(*text);
    } else if constexpr(std::is_same_v<value_t, std::string_view>) {
        detail::string_view_visitor<Deserializer> visitor{};
        auto text = deserializer.deserialize_str(visitor);
        if(!text) {
            return std::unexpected(text.error());
        }
        return *text;
    } else if constexpr(std::is_same_v<value_t, char>) {
        detail::char_visitor<Deserializer> visitor{};
        return deserializer.deserialize_char(visitor);
    } else if constexpr(std::is_same_v<value_t, bool>) {
        detail::bool_visitor<Deserializer> visitor{};
        return deserializer.deserialize_bool(visitor);
    } else if constexpr(std::is_integral_v<value_t>) {
        if constexpr(std::is_signed_v<value_t>) {
            detail::i64_visitor<Deserializer> visitor{};
            auto parsed = deserializer.deserialize_i(visitor);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            return static_cast<value_t>(*parsed);
        } else {
            detail::u64_visitor<Deserializer> visitor{};
            auto parsed = deserializer.deserialize_u(visitor);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            return static_cast<value_t>(*parsed);
        }
    } else if constexpr(std::is_floating_point_v<value_t>) {
        detail::f64_visitor<Deserializer> visitor{};
        auto parsed = deserializer.deserialize_f(visitor);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        return static_cast<value_t>(*parsed);
    } else {
        static_assert(
            detail::always_false_v<T>,
            "No Deserialize implementation for this type. " "Provide deserialize_traits<T>::deserialize(D&) or static deserialize(D&).");
    }
}

}  // namespace eventide::serde
