#pragma once

#include <cmath>
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
#include <vector>

#include "eventide/error.h"
#include "serde/concepts.h"
#include "serde/traits.h"

#if __has_include(<simdjson.h>)
#include <simdjson.h>
#else
#error "simdjson.h not found. Enable EVENTIDE_ENABLE_LANGUAGE or add simdjson include paths."
#endif

namespace eventide::serde::json::simd {

class Serializer {
public:
    using value_type = void;
    using error_type = eventide::error;

    template <class T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    class SerializeSeq {
    public:
        explicit SerializeSeq(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        status_t serialize_element(const T& value) {
            eventide::serde::serialize(serializer, value);
            return serializer.status();
        }

        result_t<value_type> end() {
            return serializer.end_array();
        }

    private:
        Serializer& serializer;
    };

    class SerializeTuple {
    public:
        explicit SerializeTuple(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        status_t serialize_element(const T& value) {
            eventide::serde::serialize(serializer, value);
            return serializer.status();
        }

        result_t<value_type> end() {
            return serializer.end_array();
        }

    private:
        Serializer& serializer;
    };

    class SerializeMap {
    public:
        explicit SerializeMap(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        status_t serialize_key(const T& key) {
            return serializer.key(Serializer::map_key_to_string(key));
        }

        template <class T>
        status_t serialize_value(const T& value) {
            eventide::serde::serialize(serializer, value);
            return serializer.status();
        }

        template <class K, class V>
        status_t serialize_entry(const K& key, const V& value) {
            auto key_status = serialize_key(key);
            if(!key_status) {
                return key_status;
            }
            return serialize_value(value);
        }

        result_t<value_type> end() {
            return serializer.end_object();
        }

    private:
        Serializer& serializer;
    };

    class SerializeStruct {
    public:
        explicit SerializeStruct(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        status_t serialize_field(std::string_view key, const T& value) {
            auto key_status = serializer.key(key);
            if(!key_status) {
                return key_status;
            }
            eventide::serde::serialize(serializer, value);
            return serializer.status();
        }

        void skip_field(std::string_view /*key*/) {}

        result_t<value_type> end() {
            return serializer.end_object();
        }

    private:
        Serializer& serializer;
    };

    Serializer() = default;

    explicit Serializer(std::size_t initial_capacity) : builder(initial_capacity) {}

private:
    status_t begin_object() {
        if(!before_value()) {
            return status();
        }
        builder.start_object();
        stack.push_back(container_frame{container_kind::object, true, true});
        return {};
    }

    result_t<value_type> end_object() {
        if(!is_valid || stack.empty()) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        const auto frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        builder.end_object();
        stack.pop_back();
        return status();
    }

    status_t begin_array() {
        if(!before_value()) {
            return status();
        }
        builder.start_array();
        stack.push_back(container_frame{container_kind::array, true, false});
        return {};
    }

    result_t<value_type> end_array() {
        if(!is_valid || stack.empty()) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        if(stack.back().kind != container_kind::array) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        builder.end_array();
        stack.pop_back();
        return status();
    }

    status_t key(std::string_view key) {
        if(!is_valid || stack.empty()) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        auto& frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        if(!frame.first) {
            builder.append_comma();
        }
        frame.first = false;

        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        frame.expect_key = false;
        return status();
    }

    status_t null() {
        if(!before_value()) {
            return status();
        }
        builder.append_null();
        return status();
    }

    status_t value(std::string_view value) {
        if(!before_value()) {
            return status();
        }
        builder.escape_and_append_with_quotes(value);
        return status();
    }

    status_t value(bool value) {
        if(!before_value()) {
            return status();
        }
        builder.append(value);
        return status();
    }

    status_t value(std::int64_t value) {
        if(!before_value()) {
            return status();
        }
        builder.append(value);
        return status();
    }

    status_t value(std::uint64_t value) {
        if(!before_value()) {
            return status();
        }
        builder.append(value);
        return status();
    }

    status_t value(double value) {
        if(!before_value()) {
            return status();
        }
        if(std::isfinite(value)) {
            builder.append(value);
        } else {
            builder.append_null();
        }
        return status();
    }

    error_type current_error() const {
        if(last_error.has_error()) {
            return last_error;
        }
        return eventide::error::invalid_argument;
    }

    status_t status() const {
        if(is_valid) {
            return {};
        }
        return std::unexpected(current_error());
    }

public:
    eventide::result<std::string_view> view() const {
        if(!is_valid || !stack.empty() || !root_written) {
            return std::unexpected(eventide::error::invalid_argument);
        }

        std::string_view out{};
        auto err = builder.view().get(out);
        if(err != simdjson::SUCCESS) {
            return std::unexpected(eventide::error::not_enough_memory);
        }
        return out;
    }

    eventide::result<std::string> str() const {
        auto out = view();
        if(!out) {
            return std::unexpected(out.error());
        }
        return std::string(*out);
    }

    void clear() {
        builder.clear();
        stack.clear();
        root_written = false;
        is_valid = true;
        last_error = error_type{};
    }

    bool valid() const {
        return is_valid;
    }

    error_type error() const {
        if(is_valid) {
            return error_type{};
        }
        return current_error();
    }

    result_t<value_type> serialize_none() {
        return null();
    }

    result_t<value_type> serialize_bool(bool value) {
        return this->value(value);
    }

    result_t<value_type> serialize_int(std::int64_t value) {
        return this->value(value);
    }

    result_t<value_type> serialize_uint(std::uint64_t value) {
        return this->value(value);
    }

    result_t<value_type> serialize_float(double value) {
        return this->value(value);
    }

    result_t<value_type> serialize_char(char value) {
        if(!before_value()) {
            return status();
        }
        builder.escape_and_append_with_quotes(value);
        return status();
    }

    result_t<value_type> serialize_str(std::string_view value) {
        return this->value(value);
    }

    result_t<value_type> serialize_bytes(std::string_view value) {
        auto seq = serialize_seq(value.size());
        if(!seq) {
            return std::unexpected(seq.error());
        }

        for(unsigned char byte: value) {
            auto element = seq->serialize_element(static_cast<std::uint64_t>(byte));
            if(!element) {
                return std::unexpected(element.error());
            }
        }
        return seq->end();
    }

    template <std::signed_integral T>
    void serialize_i(T value) {
        static_assert(sizeof(T) <= sizeof(std::int64_t),
                      "serialize_i only supports up to 64-bit signed integers.");
        (void)serialize_int(static_cast<std::int64_t>(value));
    }

    template <std::unsigned_integral T>
    void serialize_u(T value) {
        static_assert(sizeof(T) <= sizeof(std::uint64_t),
                      "serialize_u only supports up to 64-bit unsigned integers.");
        (void)serialize_uint(static_cast<std::uint64_t>(value));
    }

    template <std::floating_point T>
    void serialize_f(T value) {
        (void)serialize_float(static_cast<double>(value));
    }

    void serialize_bytes(std::span<const std::byte> value) {
        if(value.empty()) {
            (void)serialize_bytes(std::string_view{});
            return;
        }
        const char* bytes = reinterpret_cast<const char*>(value.data());
        (void)serialize_bytes(std::string_view(bytes, value.size()));
    }

    template <class T>
    void serialize_some(const T& value) {
        eventide::serde::serialize(*this, value);
    }

    void serialize_unit() {
        (void)serialize_none();
    }

    result_t<SerializeSeq> serialize_seq(std::optional<std::size_t> /*len*/) {
        auto started = begin_array();
        if(!started) {
            return std::unexpected(started.error());
        }
        return SerializeSeq(*this);
    }

    result_t<SerializeTuple> serialize_tuple(std::size_t /*len*/) {
        auto started = begin_array();
        if(!started) {
            return std::unexpected(started.error());
        }
        return SerializeTuple(*this);
    }

    result_t<SerializeMap> serialize_map(std::optional<std::size_t> /*len*/) {
        auto started = begin_object();
        if(!started) {
            return std::unexpected(started.error());
        }
        return SerializeMap(*this);
    }

    result_t<SerializeStruct> serialize_struct(std::string_view /*name*/, std::size_t /*len*/) {
        auto started = begin_object();
        if(!started) {
            return std::unexpected(started.error());
        }
        return SerializeStruct(*this);
    }

    SerializeStruct serialize_struct(const char* name, std::size_t len) {
        auto object =
            serialize_struct(name == nullptr ? std::string_view{} : std::string_view(name), len);
        if(!object) {
            return SerializeStruct(*this);
        }
        return *object;
    }

    bool is_human_readable() const {
        return true;
    }

private:
    enum class container_kind { array, object };

    struct container_frame {
        container_kind kind;
        bool first = true;
        bool expect_key = true;
    };

    void set_error(error_type error) {
        if(!last_error.has_error()) {
            last_error = error;
        }
    }

    void mark_invalid(error_type error = eventide::error::invalid_argument) {
        is_valid = false;
        set_error(error);
    }

    bool before_value() {
        if(!is_valid) {
            return false;
        }

        if(stack.empty()) {
            if(root_written) {
                mark_invalid();
                return false;
            }
            root_written = true;
            return true;
        }

        auto& frame = stack.back();
        if(frame.kind == container_kind::array) {
            if(!frame.first) {
                builder.append_comma();
            }
            frame.first = false;
            return true;
        }

        if(frame.expect_key) {
            mark_invalid();
            return false;
        }

        frame.expect_key = true;
        return true;
    }

    template <class T>
    static std::string map_key_to_string(const T& key) {
        using key_t = std::remove_cvref_t<T>;
        if constexpr(std::is_same_v<key_t, const char*> || std::is_same_v<key_t, char*>) {
            return key == nullptr ? std::string() : std::string(key);
        } else if constexpr(std::is_array_v<key_t> &&
                            (std::is_same_v<std::remove_extent_t<key_t>, const char> ||
                             std::is_same_v<std::remove_extent_t<key_t>, char>)) {
            return std::string(std::string_view(key));
        } else {
            return eventide::serde::detail::key_to_string(key);
        }
    }

    simdjson::builder::string_builder builder{};
    std::vector<container_frame> stack{};
    bool root_written = false;
    bool is_valid = true;
    error_type last_error{};
};

using SerializeSeq = Serializer::SerializeSeq;
using SerializeTuple = Serializer::SerializeTuple;
using SerializeMap = Serializer::SerializeMap;
using SerializeStruct = Serializer::SerializeStruct;

static_assert(eventide::serde::serializer_like<Serializer>);

}  // namespace eventide::serde::json::simd
