#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "serde/traits.h"

#if __has_include(<simdjson.h>)
#include <simdjson.h>
#else
#error "simdjson.h not found. Enable EVENTIDE_ENABLE_LANGUAGE or add simdjson include paths."
#endif

namespace eventide::serde::detail {

template <>
struct error_traits<simdjson::error_code> {
    constexpr static simdjson::error_code invalid_argument() {
        return simdjson::INCORRECT_TYPE;
    }

    constexpr static simdjson::error_code no_data_available() {
        return simdjson::NO_SUCH_FIELD;
    }
};

}  // namespace eventide::serde::detail

namespace eventide::serde::json::simd {

class Deserializer {
public:
    using value_type = simdjson::dom::element;
    using object_type = simdjson::dom::object;
    using array_type = simdjson::dom::array;
    using error_type = simdjson::error_code;

    template <class T>
    using result_t = std::expected<T, error_type>;

    template <class Visitor>
    constexpr static bool is_visitor_v = requires(const Visitor& visitor) {
        typename Visitor::value_type;
        { visitor.expecting() } -> std::convertible_to<std::string_view>;
    };

    template <class Visitor>
    using visitor_result_t = result_t<typename Visitor::value_type>;

    class SeqAccess {
    public:
        SeqAccess(Deserializer& deserializer, array_type array) :
            deserializer_(deserializer), it_(array.begin()), end_(array.end()) {}

        template <class T>
        result_t<std::optional<T>> next_element() {
            if(it_ == end_) {
                return std::optional<T>{};
            }
            auto value = *it_++;
            auto parsed = eventide::serde::deserialize<T>(deserializer_, value);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            return std::optional<T>(std::move(*parsed));
        }

    private:
        Deserializer& deserializer_;
        array_type::iterator it_;
        array_type::iterator end_;
    };

    class MapAccess {
    public:
        MapAccess(Deserializer& deserializer, object_type object) :
            deserializer_(deserializer), it_(object.begin()), end_(object.end()) {}

        template <class K>
        result_t<std::optional<K>> next_key() {
            if(expect_value_) {
                return std::unexpected(invalid_argument_error());
            }
            if(it_ == end_) {
                return std::optional<K>{};
            }

            pending_key_ = (*it_).key;
            pending_value_ = (*it_).value;
            expect_value_ = true;

            K out{};
            if constexpr(std::is_same_v<std::remove_cvref_t<K>, std::string>) {
                out = std::string(pending_key_);
            } else if constexpr(std::is_same_v<std::remove_cvref_t<K>, std::string_view>) {
                out = pending_key_;
            } else if constexpr(std::is_integral_v<std::remove_cvref_t<K>>) {
                if(!eventide::serde::detail::parse_key(pending_key_, out)) {
                    return std::unexpected(invalid_argument_error());
                }
            } else {
                static_assert(always_false_v<K>, "Map key type must be string-like or integral.");
            }

            return std::optional<K>(std::move(out));
        }

        template <class V>
        result_t<V> next_value() {
            if(!expect_value_) {
                return std::unexpected(invalid_argument_error());
            }

            expect_value_ = false;
            auto value = pending_value_;
            ++it_;
            return eventide::serde::deserialize<V>(deserializer_, value);
        }

    private:
        template <class>
        constexpr static bool always_false_v = false;

        Deserializer& deserializer_;
        object_type::iterator it_;
        object_type::iterator end_;
        bool expect_value_ = false;
        std::string_view pending_key_{};
        value_type pending_value_{};
    };

    Deserializer() = default;

    explicit Deserializer(std::string_view json) {
        parse(json);
    }

    result_t<void> parse(std::string_view json) {
        value_type parsed{};
        auto err = std::move(parser_.parse(json.data(), json.size())).get(parsed);
        if(err != simdjson::SUCCESS) {
            has_root_ = false;
            return std::unexpected(err);
        }
        root_ = parsed;
        has_root_ = true;
        return {};
    }

    result_t<void> parse(const std::string& json) {
        value_type parsed{};
        auto err = std::move(parser_.parse(json)).get(parsed);
        if(err != simdjson::SUCCESS) {
            has_root_ = false;
            return std::unexpected(err);
        }
        root_ = parsed;
        has_root_ = true;
        return {};
    }

    result_t<value_type> root() const {
        if(!has_root_) {
            return std::unexpected(simdjson::EMPTY);
        }
        return root_;
    }

    bool has_root() const {
        return has_root_;
    }

private:
    template <class T, class D>
    friend eventide::serde::deserialize_result_t<T, D>
        eventide::serde::deserialize(D& deserializer, value_type_t<D> value);

    result_t<object_type> as_object(value_type value) const {
        return from_simd(std::move(value.get_object()));
    }

    result_t<array_type> as_array(value_type value) const {
        return from_simd(std::move(value.get_array()));
    }

    result_t<std::string_view> as_string(value_type value) const {
        return from_simd(std::move(value.get_string()));
    }

    result_t<std::int64_t> as_i64(value_type value) const {
        return from_simd(std::move(value.get_int64()));
    }

    result_t<std::uint64_t> as_u64(value_type value) const {
        return from_simd(std::move(value.get_uint64()));
    }

    result_t<double> as_f64(value_type value) const {
        return from_simd(std::move(value.get_double()));
    }

    result_t<bool> as_bool(value_type value) const {
        return from_simd(std::move(value.get_bool()));
    }

    bool is_null(value_type value) const {
        return value.is_null();
    }

    result_t<value_type> find(object_type object, std::string_view key) const {
        auto found = std::move(object.at_key(key));
        value_type out{};
        auto err = std::move(found).get(out);
        if(err != simdjson::SUCCESS) {
            return std::unexpected(err);
        }
        return out;
    }

    template <class Fn>
    result_t<void> for_each_member(object_type object, Fn&& fn) const {
        for(auto field: object) {
            auto result = fn(field.key, field.value);
            if(!result) {
                return std::unexpected(result.error());
            }
        }
        return {};
    }

    template <class Fn>
    result_t<void> for_each_element(array_type array, Fn&& fn) const {
        for(auto element: array) {
            auto result = fn(element);
            if(!result) {
                return std::unexpected(result.error());
            }
        }
        return {};
    }

public:
    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_any(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_any(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_bool(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_bool(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_i(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_i(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_u(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_u(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_f(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_f(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_char(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_char(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_str(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_str(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_string(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_string(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_bytes(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_bytes(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_byte_buf(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_byte_buf(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_option(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_option(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_unit(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_unit(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_seq(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_seq(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_tuple(std::size_t /*len*/, Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_seq(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_map(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_map(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_struct(std::string_view /*name*/,
                                                 std::span<const std::string_view> /*fields*/,
                                                 Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_map(visitor, *value);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_identifier(Visitor& visitor) {
        return deserialize_str(visitor);
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_ignored_any(Visitor& visitor) {
        auto value = root();
        if(!value) {
            return std::unexpected(value.error());
        }
        return deserialize_ignored_any(visitor, *value);
    }

    bool is_human_readable() const {
        return true;
    }

private:
    template <class T>
    static result_t<T> from_simd(simdjson::simdjson_result<T> value) {
        T out{};
        auto err = std::move(value).get(out);
        if(err != simdjson::SUCCESS) {
            return std::unexpected(err);
        }
        return out;
    }

    constexpr static error_type invalid_argument_error() {
        return eventide::serde::detail::error_traits<error_type>::invalid_argument();
    }

    template <class Visitor>
    static visitor_result_t<Visitor> invalid_type() {
        return std::unexpected(invalid_argument_error());
    }

    result_t<std::vector<std::byte>> as_owned_bytes(value_type value) const {
        if(value.is_string()) {
            auto text = as_string(value);
            if(!text) {
                return std::unexpected(text.error());
            }
            std::vector<std::byte> out;
            out.reserve(text->size());
            for(char ch: *text) {
                out.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
            }
            return out;
        }

        auto array = as_array(value);
        if(!array) {
            return std::unexpected(array.error());
        }

        std::vector<std::byte> out;
        out.reserve(array->size());
        for(auto element: *array) {
            auto number = as_u64(element);
            if(!number || *number > std::numeric_limits<std::uint8_t>::max()) {
                return std::unexpected(simdjson::NUMBER_OUT_OF_RANGE);
            }
            out.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(*number)));
        }
        return out;
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_any(Visitor& visitor, value_type value) {
        switch(value.type()) {
            case simdjson::dom::element_type::OBJECT: return deserialize_map(visitor, value);
            case simdjson::dom::element_type::ARRAY: return deserialize_seq(visitor, value);
            case simdjson::dom::element_type::INT64: return deserialize_i(visitor, value);
            case simdjson::dom::element_type::UINT64: return deserialize_u(visitor, value);
            case simdjson::dom::element_type::DOUBLE: return deserialize_f(visitor, value);
            case simdjson::dom::element_type::STRING: return deserialize_string(visitor, value);
            case simdjson::dom::element_type::BOOL: return deserialize_bool(visitor, value);
            case simdjson::dom::element_type::NULL_VALUE: return deserialize_option(visitor, value);
        }
        return invalid_type<Visitor>();
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_bool(Visitor& visitor, value_type value) {
        auto parsed = as_bool(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        if constexpr(requires(Visitor& v, bool b) {
                         { v.visit_bool(b) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_bool(*parsed);
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_i(Visitor& visitor, value_type value) {
        auto parsed = as_i64(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        if constexpr(requires(Visitor& v, std::int64_t i) {
                         { v.visit_i64(i) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_i64(*parsed);
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_u(Visitor& visitor, value_type value) {
        auto parsed = as_u64(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        if constexpr(requires(Visitor& v, std::uint64_t u) {
                         { v.visit_u64(u) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_u64(*parsed);
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_f(Visitor& visitor, value_type value) {
        auto parsed = as_f64(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        if constexpr(requires(Visitor& v, double f) {
                         { v.visit_f64(f) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_f64(*parsed);
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_char(Visitor& visitor, value_type value) {
        auto parsed = as_string(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        if(parsed->size() != 1) {
            return std::unexpected(invalid_argument_error());
        }
        if constexpr(requires(Visitor& v, char c) {
                         { v.visit_char(c) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_char((*parsed)[0]);
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_str(Visitor& visitor, value_type value) {
        static_assert(
            requires(Visitor& v, std::string_view s) {
                { v.visit_str(s) } -> std::same_as<visitor_result_t<Visitor>>;
            },
            "Visitor for deserialize_str must implement visit_str(std::string_view).");

        auto parsed = as_string(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        if constexpr(requires(Visitor& v, std::string_view s) {
                         { v.visit_borrowed_str(s) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_borrowed_str(*parsed);
        } else {
            return visitor.visit_str(*parsed);
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_string(Visitor& visitor, value_type value) {
        auto parsed = as_string(value);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        if constexpr(requires(Visitor& v, std::string s) {
                         {
                             v.visit_string(std::move(s))
                         } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_string(std::string(*parsed));
        } else {
            return deserialize_str(visitor, value);
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_bytes(Visitor& visitor, value_type value) {
        static_assert(
            requires(Visitor& v, std::span<const std::byte> b) {
                { v.visit_bytes(b) } -> std::same_as<visitor_result_t<Visitor>>;
            },
            "Visitor for deserialize_bytes must implement visit_bytes(span<const byte>).");

        if(value.is_string()) {
            auto parsed = as_string(value);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }
            const auto span =
                std::span(reinterpret_cast<const std::byte*>(parsed->data()), parsed->size());
            if constexpr(requires(Visitor& v, std::span<const std::byte> b) {
                             {
                                 v.visit_borrowed_bytes(b)
                             } -> std::same_as<visitor_result_t<Visitor>>;
                         }) {
                return visitor.visit_borrowed_bytes(span);
            } else {
                return visitor.visit_bytes(span);
            }
        }

        auto bytes = as_owned_bytes(value);
        if(!bytes) {
            return std::unexpected(bytes.error());
        }
        return visitor.visit_bytes(std::span<const std::byte>(bytes->data(), bytes->size()));
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_byte_buf(Visitor& visitor, value_type value) {
        auto bytes = as_owned_bytes(value);
        if(!bytes) {
            return std::unexpected(bytes.error());
        }

        if constexpr(requires(Visitor& v, std::vector<std::byte> b) {
                         {
                             v.visit_byte_buf(std::move(b))
                         } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_byte_buf(std::move(*bytes));
        } else {
            return deserialize_bytes(visitor, value);
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_option(Visitor& visitor, value_type value) {
        if(is_null(value)) {
            if constexpr(requires(Visitor& v) {
                             { v.visit_none() } -> std::same_as<visitor_result_t<Visitor>>;
                         }) {
                return visitor.visit_none();
            }
            return deserialize_unit(visitor, value);
        }

        if constexpr(requires(Visitor& v, Deserializer& d, value_type val) {
                         { v.visit_some(d, val) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_some(*this, value);
        } else if constexpr(requires(Visitor& v, value_type val) {
                                { v.visit_some(val) } -> std::same_as<visitor_result_t<Visitor>>;
                            }) {
            return visitor.visit_some(value);
        } else {
            return deserialize_any(visitor, value);
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_unit(Visitor& visitor, value_type value) {
        if(!is_null(value)) {
            return std::unexpected(invalid_argument_error());
        }
        if constexpr(requires(Visitor& v) {
                         { v.visit_unit() } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_unit();
        } else if constexpr(requires(Visitor& v) {
                                { v.visit_none() } -> std::same_as<visitor_result_t<Visitor>>;
                            }) {
            return visitor.visit_none();
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_seq(Visitor& visitor, value_type value) {
        auto array = as_array(value);
        if(!array) {
            return std::unexpected(array.error());
        }

        if constexpr(requires(Visitor& v, SeqAccess& access) {
                         { v.visit_seq(access) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            SeqAccess access(*this, *array);
            return visitor.visit_seq(access);
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_map(Visitor& visitor, value_type value) {
        auto object = as_object(value);
        if(!object) {
            return std::unexpected(object.error());
        }

        if constexpr(requires(Visitor& v, MapAccess& access) {
                         { v.visit_map(access) } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            MapAccess access(*this, *object);
            return visitor.visit_map(access);
        } else {
            return invalid_type<Visitor>();
        }
    }

    template <class Visitor>
        requires (is_visitor_v<Visitor>)
    visitor_result_t<Visitor> deserialize_ignored_any(Visitor& visitor, value_type /*value*/) {
        if constexpr(requires(Visitor& v) {
                         { v.visit_ignored_any() } -> std::same_as<visitor_result_t<Visitor>>;
                     }) {
            return visitor.visit_ignored_any();
        } else if constexpr(requires(Visitor& v) {
                                { v.visit_unit() } -> std::same_as<visitor_result_t<Visitor>>;
                            }) {
            return visitor.visit_unit();
        } else {
            return invalid_type<Visitor>();
        }
    }

    simdjson::dom::parser parser_{};
    value_type root_{};
    bool has_root_ = false;
};

}  // namespace eventide::serde::json::simd
