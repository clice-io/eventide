#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
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
        explicit SerializeSeq(Serializer& serializer) noexcept : serializer_(serializer) {}

        template <class T>
        void serialize_element(const T& value) {
            eventide::serde::serialize(serializer_, value);
        }

        void end() {
            serializer_.end_array();
        }

    private:
        Serializer& serializer_;
    };

    class SerializeTuple {
    public:
        explicit SerializeTuple(Serializer& serializer) noexcept : serializer_(serializer) {}

        template <class T>
        void serialize_element(const T& value) {
            eventide::serde::serialize(serializer_, value);
        }

        void end() {
            serializer_.end_array();
        }

    private:
        Serializer& serializer_;
    };

    class SerializeMap {
    public:
        explicit SerializeMap(Serializer& serializer) noexcept : serializer_(serializer) {}

        template <class T>
        void serialize_key(const T& key) {
            serializer_.key(Serializer::map_key_to_string(key));
        }

        template <class T>
        void serialize_value(const T& value) {
            eventide::serde::serialize(serializer_, value);
        }

        template <class K, class V>
        void serialize_entry(const K& key, const V& value) {
            serialize_key(key);
            serialize_value(value);
        }

        void end() {
            serializer_.end_object();
        }

    private:
        Serializer& serializer_;
    };

    class SerializeStruct {
    public:
        explicit SerializeStruct(Serializer& serializer) noexcept : serializer_(serializer) {}

        template <class T>
        void serialize_field(std::string_view key, const T& value) {
            serializer_.key(key);
            eventide::serde::serialize(serializer_, value);
        }

        void skip_field(std::string_view /*key*/) {}

        void end() {
            serializer_.end_object();
        }

    private:
        Serializer& serializer_;
    };

    Serializer() = default;

    explicit Serializer(std::size_t initial_capacity) : builder_(initial_capacity) {}

private:
    void begin_object() {
        if(!before_value()) {
            return;
        }
        builder_.start_object();
        stack_.push_back(container_frame{container_kind::object, true, true});
    }

    void end_object() {
        if(!valid_ || stack_.empty()) {
            mark_invalid();
            return;
        }

        const auto frame = stack_.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return;
        }

        builder_.end_object();
        stack_.pop_back();
    }

    void begin_array() {
        if(!before_value()) {
            return;
        }
        builder_.start_array();
        stack_.push_back(container_frame{container_kind::array, true, false});
    }

    void end_array() {
        if(!valid_ || stack_.empty()) {
            mark_invalid();
            return;
        }

        if(stack_.back().kind != container_kind::array) {
            mark_invalid();
            return;
        }

        builder_.end_array();
        stack_.pop_back();
    }

    void key(std::string_view key) {
        if(!valid_ || stack_.empty()) {
            mark_invalid();
            return;
        }

        auto& frame = stack_.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return;
        }

        if(!frame.first) {
            builder_.append_comma();
        }
        frame.first = false;

        builder_.escape_and_append_with_quotes(key);
        builder_.append_colon();
        frame.expect_key = false;
    }

    void null() {
        if(!before_value()) {
            return;
        }
        builder_.append_null();
    }

    void value(std::string_view value) {
        if(!before_value()) {
            return;
        }
        builder_.escape_and_append_with_quotes(value);
    }

    void value(bool value) {
        if(!before_value()) {
            return;
        }
        builder_.append(value);
    }

    void value(std::int64_t value) {
        if(!before_value()) {
            return;
        }
        builder_.append(value);
    }

    void value(std::uint64_t value) {
        if(!before_value()) {
            return;
        }
        builder_.append(value);
    }

    void value(double value) {
        if(!before_value()) {
            return;
        }
        if(std::isfinite(value)) {
            builder_.append(value);
        } else {
            builder_.append_null();
        }
    }

public:
    eventide::result<std::string_view> view() const {
        if(!valid_ || !stack_.empty() || !root_written_) {
            return std::unexpected(eventide::error::invalid_argument);
        }

        std::string_view out{};
        auto err = builder_.view().get(out);
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
        builder_.clear();
        stack_.clear();
        root_written_ = false;
        valid_ = true;
    }

    bool valid() const {
        return valid_;
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
        builder_.escape_and_append_with_quotes(value);
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

    template <class Seq>
    void collect_seq(const Seq& seq) {
        std::optional<std::size_t> len{};
        if constexpr(requires { seq.size(); }) {
            len = static_cast<std::size_t>(seq.size());
        }

        auto serializer = serialize_seq(len);
        for(const auto& item: seq) {
            serializer.serialize_element(item);
        }
        serializer.end();
    }

    template <class Map>
    void collect_map(const Map& map) {
        std::optional<std::size_t> len{};
        if constexpr(requires { map.size(); }) {
            len = static_cast<std::size_t>(map.size());
        }

        auto serializer = serialize_map(len);
        for(const auto& [key, value]: map) {
            serializer.serialize_entry(key, value);
        }
        serializer.end();
    }

    template <class T>
    void collect_str(const T& value) {
        using value_t = std::remove_cvref_t<T>;

        if constexpr(std::is_same_v<value_t, bool>) {
            serialize_str(value ? "true" : "false");
        } else if constexpr(std::is_same_v<value_t, char>) {
            serialize_char(value);
        } else if constexpr(std::is_convertible_v<value_t, std::string_view>) {
            serialize_str(std::string_view(value));
        } else if constexpr(std::is_integral_v<value_t>) {
            std::array<char, 64> buffer{};
            auto [ptr, err] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if(err == std::errc{}) {
                serialize_str(
                    std::string_view(buffer.data(), static_cast<std::size_t>(ptr - buffer.data())));
            } else {
                serialize_none();
            }
        } else if constexpr(std::is_floating_point_v<value_t>) {
            if(!std::isfinite(value)) {
                serialize_none();
                return;
            }

            std::array<char, 64> buffer{};
            auto [ptr, err] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if(err == std::errc{}) {
                serialize_str(
                    std::string_view(buffer.data(), static_cast<std::size_t>(ptr - buffer.data())));
            } else {
                serialize_none();
            }
        } else if constexpr(requires(std::ostream& out, const value_t& v) { out << v; }) {
            std::ostringstream out;
            out << value;
            serialize_str(out.str());
        } else {
            static_assert(always_false_v<T>,
                          "collect_str requires string-like or streamable type.");
        }
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

    template <class>
    constexpr static bool always_false_v = false;

    void mark_invalid() {
        valid_ = false;
    }

    bool before_value() {
        if(!valid_) {
            return false;
        }

        if(stack_.empty()) {
            if(root_written_) {
                mark_invalid();
                return false;
            }
            root_written_ = true;
            return true;
        }

        auto& frame = stack_.back();
        if(frame.kind == container_kind::array) {
            if(!frame.first) {
                builder_.append_comma();
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

    simdjson::builder::string_builder builder_{};
    std::vector<container_frame> stack_{};
    bool root_written_ = false;
    bool valid_ = true;
};

using SerializeSeq = Serializer::SerializeSeq;
using SerializeTuple = Serializer::SerializeTuple;
using SerializeMap = Serializer::SerializeMap;
using SerializeStruct = Serializer::SerializeStruct;

}  // namespace eventide::serde::json::simd
