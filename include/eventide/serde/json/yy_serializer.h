#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "eventide/serde/json/dom.h"
#include "eventide/serde/json/error.h"
#include "eventide/serde/serde.h"

namespace eventide::serde::json::yy {

class Serializer {
public:
    using value_type = void;
    using error_type = json::error_kind;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    class SerializeArray {
    public:
        explicit SerializeArray(Serializer& serializer) noexcept : serializer(serializer) {}

        template <typename T>
        status_t serialize_element(const T& value) {
            auto result = serde::serialize(serializer, value);
            if(!result) {
                return std::unexpected(result.error());
            }
            return {};
        }

        result_t<value_type> end() {
            return serializer.end_array();
        }

    private:
        Serializer& serializer;
    };

    class SerializeObject {
    public:
        explicit SerializeObject(Serializer& serializer) noexcept : serializer(serializer) {}

        template <typename K, typename V>
        status_t serialize_entry(const K& key, const V& value) {
            auto key_status = serializer.key(serde::spelling::map_key_to_string(key));
            if(!key_status) {
                return std::unexpected(key_status.error());
            }
            return serde::serialize(serializer, value);
        }

        template <typename T>
        status_t serialize_field(std::string_view key, const T& value) {
            auto key_status = serializer.key(key);
            if(!key_status) {
                return std::unexpected(key_status.error());
            }
            return serde::serialize(serializer, value);
        }

        result_t<value_type> end() {
            return serializer.end_object();
        }

    private:
        Serializer& serializer;
    };

    using SerializeSeq = SerializeArray;
    using SerializeTuple = SerializeArray;
    using SerializeMap = SerializeObject;
    using SerializeStruct = SerializeObject;

    Serializer() : doc(yyjson_mut_doc_new(nullptr), yyjson_mut_doc_free) {
        if(doc == nullptr) {
            mark_invalid(error_type::allocation_failed);
        }
    }

    [[nodiscard]] bool valid() const noexcept {
        return isValid;
    }

    [[nodiscard]] error_type error() const noexcept {
        return lastError;
    }

    result_t<json::Value> dom_value() const {
        if(!isValid || !rootWritten || !stack.empty()) {
            return std::unexpected(current_error());
        }

        auto value = json::Value::from_mutable_doc(doc);
        if(!value) {
            return std::unexpected(error_type::invalid_state);
        }
        return std::move(*value);
    }

    result_t<std::string> str() const {
        if(!isValid || !rootWritten || !stack.empty()) {
            return std::unexpected(current_error());
        }

        yyjson_write_err err{};
        size_t len = 0;
        auto* root = yyjson_mut_doc_get_root(doc.get());
        char* out = yyjson_mut_val_write_opts(root, YYJSON_WRITE_NOFLAG, nullptr, &len, &err);
        if(out == nullptr) {
            return std::unexpected(error_type::write_failed);
        }

        std::string json(out, len);
        std::free(out);
        return json;
    }

    result_t<value_type> serialize_none() {
        return append_created(yyjson_mut_null(doc.get()));
    }

    template <typename T>
    result_t<value_type> serialize_some(const T& value) {
        return serde::serialize(*this, value);
    }

    template <typename... Ts>
    result_t<value_type> serialize_variant(const std::variant<Ts...>& value) {
        return std::visit(
            [&](const auto& item) -> result_t<value_type> { return serde::serialize(*this, item); },
            value);
    }

    result_t<value_type> serialize_bool(bool value) {
        return append_created(yyjson_mut_bool(doc.get(), value));
    }

    result_t<value_type> serialize_int(std::int64_t value) {
        return append_created(yyjson_mut_sint(doc.get(), value));
    }

    result_t<value_type> serialize_uint(std::uint64_t value) {
        return append_created(yyjson_mut_uint(doc.get(), value));
    }

    result_t<value_type> serialize_float(double value) {
        if(std::isfinite(value)) {
            return append_created(yyjson_mut_real(doc.get(), value));
        }
        return serialize_none();
    }

    result_t<value_type> serialize_char(char value) {
        const char chars[1] = {value};
        return append_created(yyjson_mut_strncpy(doc.get(), chars, 1));
    }

    result_t<value_type> serialize_str(std::string_view value) {
        return append_created(yyjson_mut_strncpy(doc.get(), value.data(), value.size()));
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

    result_t<value_type> serialize_bytes(std::span<const std::byte> value) {
        auto seq = serialize_seq(value.size());
        if(!seq) {
            return std::unexpected(seq.error());
        }

        for(std::byte byte: value) {
            auto element = seq->serialize_element(
                static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(byte)));
            if(!element) {
                return std::unexpected(element.error());
            }
        }
        return seq->end();
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

    result_t<value_type> append_json_value(const json::Value& value) {
        if(!isValid) {
            return std::unexpected(current_error());
        }

        yyjson_mut_val* copied = nullptr;
        if(value.is_mutable()) {
            copied = yyjson_mut_val_mut_copy(doc.get(),
                                             const_cast<yyjson_mut_val*>(value.mutable_value()));
        } else {
            copied =
                yyjson_val_mut_copy(doc.get(), const_cast<yyjson_val*>(value.immutable_value()));
        }
        if(copied == nullptr) {
            mark_invalid(error_type::allocation_failed);
            return std::unexpected(current_error());
        }

        auto status = append_value(copied);
        if(!status) {
            return std::unexpected(status.error());
        }
        return {};
    }

private:
    enum class container_kind : std::uint8_t { array, object };

    struct container_frame {
        container_kind kind;
        yyjson_mut_val* value = nullptr;
        bool expect_key = true;
        std::string pending_key;
    };

    status_t begin_object() {
        auto* obj = yyjson_mut_obj(doc.get());
        if(obj == nullptr) {
            mark_invalid(error_type::allocation_failed);
            return std::unexpected(current_error());
        }

        auto appended = append_value(obj);
        if(!appended) {
            return std::unexpected(appended.error());
        }

        stack.push_back(container_frame{
            .kind = container_kind::object,
            .value = obj,
            .expect_key = true,
        });
        return {};
    }

    result_t<value_type> end_object() {
        if(!isValid || stack.empty()) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        const auto& frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        stack.pop_back();
        return {};
    }

    status_t begin_array() {
        auto* arr = yyjson_mut_arr(doc.get());
        if(arr == nullptr) {
            mark_invalid(error_type::allocation_failed);
            return std::unexpected(current_error());
        }

        auto appended = append_value(arr);
        if(!appended) {
            return std::unexpected(appended.error());
        }

        stack.push_back(container_frame{
            .kind = container_kind::array,
            .value = arr,
            .expect_key = false,
        });
        return {};
    }

    result_t<value_type> end_array() {
        if(!isValid || stack.empty()) {
            mark_invalid();
            return std::unexpected(current_error());
        }
        if(stack.back().kind != container_kind::array) {
            mark_invalid();
            return std::unexpected(current_error());
        }
        stack.pop_back();
        return {};
    }

    status_t key(std::string_view key_name) {
        if(!isValid || stack.empty()) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        auto& frame = stack.back();
        if(frame.kind != container_kind::object || !frame.expect_key) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        frame.pending_key.assign(key_name.data(), key_name.size());
        frame.expect_key = false;
        return {};
    }

    result_t<value_type> append_created(yyjson_mut_val* value) {
        if(value == nullptr) {
            mark_invalid(error_type::allocation_failed);
            return std::unexpected(current_error());
        }

        auto status = append_value(value);
        if(!status) {
            return std::unexpected(status.error());
        }
        return {};
    }

    status_t append_value(yyjson_mut_val* value) {
        if(!isValid) {
            return std::unexpected(current_error());
        }
        if(value == nullptr) {
            mark_invalid(error_type::allocation_failed);
            return std::unexpected(current_error());
        }

        if(stack.empty()) {
            if(rootWritten) {
                mark_invalid();
                return std::unexpected(current_error());
            }
            yyjson_mut_doc_set_root(doc.get(), value);
            rootWritten = true;
            return {};
        }

        auto& frame = stack.back();
        if(frame.kind == container_kind::array) {
            if(!yyjson_mut_arr_add_val(frame.value, value)) {
                mark_invalid(error_type::allocation_failed);
                return std::unexpected(current_error());
            }
            return {};
        }

        if(frame.expect_key) {
            mark_invalid();
            return std::unexpected(current_error());
        }

        yyjson_mut_val* key_value =
            yyjson_mut_strncpy(doc.get(), frame.pending_key.data(), frame.pending_key.size());
        if(key_value == nullptr) {
            mark_invalid(error_type::allocation_failed);
            return std::unexpected(current_error());
        }

        if(!yyjson_mut_obj_put(frame.value, key_value, value)) {
            mark_invalid(error_type::allocation_failed);
            return std::unexpected(current_error());
        }

        frame.pending_key.clear();
        frame.expect_key = true;
        return {};
    }

    void mark_invalid(error_type error = error_type::invalid_state) {
        isValid = false;
        if(lastError == error_type::invalid_state || error != error_type::invalid_state) {
            lastError = error;
        }
    }

    [[nodiscard]] error_type current_error() const noexcept {
        return lastError;
    }

private:
    bool isValid = true;
    bool rootWritten = false;
    error_type lastError = error_type::invalid_state;
    std::vector<container_frame> stack;
    std::shared_ptr<yyjson_mut_doc> doc;
};

template <typename T>
auto to_dom(const T& value) -> std::expected<json::Value, Serializer::error_type> {
    Serializer serializer;
    if(!serializer.valid()) {
        return std::unexpected(serializer.error());
    }

    auto result = serde::serialize(serializer, value);
    if(!result) {
        return std::unexpected(result.error());
    }
    return serializer.dom_value();
}

template <typename T>
auto to_json(const T& value) -> std::expected<std::string, Serializer::error_type> {
    auto dom = to_dom(value);
    if(!dom) {
        return std::unexpected(dom.error());
    }

    auto json = dom->to_json_string();
    if(!json) {
        return std::unexpected(Serializer::error_type::write_failed);
    }
    return std::move(*json);
}

static_assert(serde::serializer_like<Serializer>);

}  // namespace eventide::serde::json::yy
