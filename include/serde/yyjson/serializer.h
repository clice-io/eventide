#pragma once

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
#include "serde/yyjson/dom.h"

namespace eventide::serde::json::yy {

class Serializer {
public:
    using error_type = yyjson_write_code;

    template <class T>
    using result_t = std::expected<T, error_type>;

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

    Serializer() {
        clear();
    }

    explicit Serializer(std::size_t /*initial_capacity*/) {
        clear();
    }

    ~Serializer() = default;

    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;
    Serializer(Serializer&&) = delete;
    Serializer& operator=(Serializer&&) = delete;

private:
    enum class container_kind { array, object };

    struct container_frame {
        MutableDom::value_type container = nullptr;
        container_kind kind = container_kind::array;
        bool expect_key = true;
        std::string pending_key;
    };

    bool is_complete() const {
        return is_valid && root_written && stack.empty();
    }

    void set_error(error_type error) {
        if(last_error == YYJSON_WRITE_SUCCESS) {
            last_error = error;
        }
    }

    void mark_invalid(error_type error) {
        is_valid = false;
        set_error(error);
    }

    template <class Fn>
    void append_created(Fn&& fn) {
        if(!is_valid) {
            return;
        }
        auto created = fn();
        if(!created) {
            mark_invalid(created.error());
            return;
        }
        append_value(*created);
    }

    void append_value(MutableDom::value_type value) {
        if(!is_valid) {
            return;
        }
        if(value == nullptr) {
            mark_invalid(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
            return;
        }

        if(stack.empty()) {
            if(root_written) {
                mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
                return;
            }

            auto set_root = mutable_dom.set_root_value(value);
            if(!set_root) {
                mark_invalid(set_root.error());
                return;
            }
            root_written = true;
            return;
        }

        auto& frame = stack.back();
        if(frame.kind == container_kind::array) {
            auto append = mutable_dom.append_array_value(frame.container, value);
            if(!append) {
                mark_invalid(append.error());
            }
            return;
        }

        if(frame.expect_key) {
            mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
            return;
        }

        auto append = mutable_dom.add_object_value(frame.container, frame.pending_key, value);
        if(!append) {
            mark_invalid(append.error());
            return;
        }

        frame.pending_key.clear();
        frame.expect_key = true;
    }

    void begin_object() {
        if(!is_valid) {
            return;
        }

        auto created = mutable_dom.create_object();
        if(!created) {
            mark_invalid(created.error());
            return;
        }

        auto* object = *created;
        append_value(object);
        if(!is_valid) {
            return;
        }

        stack.push_back(container_frame{
            .container = object,
            .kind = container_kind::object,
            .expect_key = true,
        });
    }

    void end_object() {
        if(!is_valid || stack.empty()) {
            mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
            return;
        }

        auto& frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
            return;
        }

        stack.pop_back();
    }

    void begin_array() {
        if(!is_valid) {
            return;
        }

        auto created = mutable_dom.create_array();
        if(!created) {
            mark_invalid(created.error());
            return;
        }

        auto* array = *created;
        append_value(array);
        if(!is_valid) {
            return;
        }

        stack.push_back(container_frame{
            .container = array,
            .kind = container_kind::array,
            .expect_key = false,
        });
    }

    void end_array() {
        if(!is_valid || stack.empty()) {
            mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
            return;
        }

        auto& frame = stack.back();
        if(frame.kind != container_kind::array) {
            mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
            return;
        }

        stack.pop_back();
    }

    void key(std::string_view key) {
        if(!is_valid || stack.empty()) {
            mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
            return;
        }

        auto& frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
            return;
        }

        frame.pending_key.assign(key.data(), key.size());
        frame.expect_key = false;
    }

    void null() {
        append_created([this] { return mutable_dom.create_null(); });
    }

    void value(std::string_view value) {
        append_created([this, value] { return mutable_dom.create_string(value); });
    }

    void value(bool value) {
        append_created([this, value] { return mutable_dom.create_bool(value); });
    }

    void value(std::int64_t value) {
        append_created([this, value] { return mutable_dom.create_i64(value); });
    }

    void value(std::uint64_t value) {
        append_created([this, value] { return mutable_dom.create_u64(value); });
    }

    void value(double value) {
        append_created([this, value] { return mutable_dom.create_f64(value); });
    }

public:
    result_t<Dom> dom() const {
        if(!is_complete()) {
            const auto error = last_error == YYJSON_WRITE_SUCCESS
                                   ? YYJSON_WRITE_ERROR_INVALID_PARAMETER
                                   : last_error;
            return std::unexpected(error);
        }
        return mutable_dom.freeze();
    }

    void clear() {
        mutable_dom.clear();
        stack.clear();
        root_written = false;
        is_valid = mutable_dom.valid();
        last_error = is_valid ? YYJSON_WRITE_SUCCESS : YYJSON_WRITE_ERROR_MEMORY_ALLOCATION;
    }

    bool valid() const {
        return is_valid;
    }

    error_type error() const {
        return last_error;
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
        const char text[2] = {value, '\0'};
        this->value(std::string_view{text, 1});
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

    MutableDom mutable_dom{};
    std::vector<container_frame> stack{};
    bool root_written = false;
    bool is_valid = false;
    error_type last_error = YYJSON_WRITE_SUCCESS;
};

using SerializeSeq = Serializer::SerializeSeq;
using SerializeTuple = Serializer::SerializeTuple;
using SerializeMap = Serializer::SerializeMap;
using SerializeStruct = Serializer::SerializeStruct;

}  // namespace eventide::serde::json::yy
