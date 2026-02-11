#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
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

namespace eventide::serde::json::simd {

class Serializer {
public:
    class SerializeSeq {
    public:
        explicit SerializeSeq(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        void serialize_element(const T& value) {
            eventide::serde::serialize(serializer, value);
        }

        void end() {
            serializer.end_array();
        }

    private:
        Serializer& serializer;
    };

    class SerializeTuple {
    public:
        explicit SerializeTuple(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        void serialize_element(const T& value) {
            eventide::serde::serialize(serializer, value);
        }

        void end() {
            serializer.end_array();
        }

    private:
        Serializer& serializer;
    };

    class SerializeMap {
    public:
        explicit SerializeMap(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        void serialize_key(const T& key) {
            serializer.key(Serializer::map_key_to_string(key));
        }

        template <class T>
        void serialize_value(const T& value) {
            eventide::serde::serialize(serializer, value);
        }

        template <class K, class V>
        void serialize_entry(const K& key, const V& value) {
            serialize_key(key);
            serialize_value(value);
        }

        void end() {
            serializer.end_object();
        }

    private:
        Serializer& serializer;
    };

    class SerializeStruct {
    public:
        explicit SerializeStruct(Serializer& serializer) noexcept : serializer(serializer) {}

        template <class T>
        void serialize_field(std::string_view key, const T& value) {
            serializer.key(key);
            eventide::serde::serialize(serializer, value);
        }

        void skip_field(std::string_view /*key*/) {}

        void end() {
            serializer.end_object();
        }

    private:
        Serializer& serializer;
    };

    Serializer() = default;

    explicit Serializer(std::size_t initial_capacity) : builder(initial_capacity) {}

private:
    void begin_object() {
        if(!before_value()) {
            return;
        }
        builder.start_object();
        stack.push_back(container_frame{container_kind::object, true, true});
    }

    void end_object() {
        if(!is_valid || stack.empty()) {
            mark_invalid();
            return;
        }

        const auto frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return;
        }

        builder.end_object();
        stack.pop_back();
    }

    void begin_array() {
        if(!before_value()) {
            return;
        }
        builder.start_array();
        stack.push_back(container_frame{container_kind::array, true, false});
    }

    void end_array() {
        if(!is_valid || stack.empty()) {
            mark_invalid();
            return;
        }

        if(stack.back().kind != container_kind::array) {
            mark_invalid();
            return;
        }

        builder.end_array();
        stack.pop_back();
    }

    void key(std::string_view key) {
        if(!is_valid || stack.empty()) {
            mark_invalid();
            return;
        }

        auto& frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return;
        }

        if(!frame.first) {
            builder.append_comma();
        }
        frame.first = false;

        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        frame.expect_key = false;
    }

    void null() {
        if(!before_value()) {
            return;
        }
        builder.append_null();
    }

    void value(std::string_view value) {
        if(!before_value()) {
            return;
        }
        builder.escape_and_append_with_quotes(value);
    }

    void value(bool value) {
        if(!before_value()) {
            return;
        }
        builder.append(value);
    }

    void value(std::int64_t value) {
        if(!before_value()) {
            return;
        }
        builder.append(value);
    }

    void value(std::uint64_t value) {
        if(!before_value()) {
            return;
        }
        builder.append(value);
    }

    void value(double value) {
        if(!before_value()) {
            return;
        }
        if(std::isfinite(value)) {
            builder.append(value);
        } else {
            builder.append_null();
        }
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
    }

    bool valid() const {
        return is_valid;
    }

    void serialize_bool(bool value) {
        this->value(value);
    }

    template <std::signed_integral T>
    void serialize_i(T value) {
        static_assert(sizeof(T) <= sizeof(std::int64_t),
                      "serialize_i only supports up to 64-bit signed integers.");
        this->value(static_cast<std::int64_t>(value));
    }

    template <std::unsigned_integral T>
    void serialize_u(T value) {
        static_assert(sizeof(T) <= sizeof(std::uint64_t),
                      "serialize_u only supports up to 64-bit unsigned integers.");
        this->value(static_cast<std::uint64_t>(value));
    }

    template <std::floating_point T>
    void serialize_f(T value) {
        this->value(static_cast<double>(value));
    }

    void serialize_char(char value) {
        if(!before_value()) {
            return;
        }
        builder.escape_and_append_with_quotes(value);
    }

    void serialize_str(std::string_view value) {
        this->value(value);
    }

    void serialize_bytes(std::span<const std::byte> value) {
        auto seq = serialize_seq(value.size());
        for(std::byte byte: value) {
            seq.serialize_element(std::to_integer<std::uint8_t>(byte));
        }
        seq.end();
    }

    void serialize_none() {
        null();
    }

    template <class T>
    void serialize_some(const T& value) {
        eventide::serde::serialize(*this, value);
    }

    void serialize_unit() {
        null();
    }

    SerializeSeq serialize_seq(std::optional<std::size_t> /*len*/) {
        begin_array();
        return SerializeSeq(*this);
    }

    SerializeTuple serialize_tuple(std::size_t /*len*/) {
        begin_array();
        return SerializeTuple(*this);
    }

    SerializeMap serialize_map(std::optional<std::size_t> /*len*/) {
        begin_object();
        return SerializeMap(*this);
    }

    SerializeStruct serialize_struct(std::string_view /*name*/, std::size_t /*len*/) {
        begin_object();
        return SerializeStruct(*this);
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

    void mark_invalid() {
        is_valid = false;
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
};

using SerializeSeq = Serializer::SerializeSeq;
using SerializeTuple = Serializer::SerializeTuple;
using SerializeMap = Serializer::SerializeMap;
using SerializeStruct = Serializer::SerializeStruct;

}  // namespace eventide::serde::json::simd
