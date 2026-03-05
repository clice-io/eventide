#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "eventide/serde/detail/deserialize_helpers.h"
#include "eventide/serde/detail/narrow.h"
#include "eventide/serde/json/dom.h"
#include "eventide/serde/json/error.h"
#include "eventide/serde/serde.h"
#include "eventide/serde/variant.h"

namespace eventide::serde::json::yy {

class Deserializer {
public:
    using error_type = json::error_kind;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    class DeserializeArray :
        public serde::detail::IndexedArrayDeserializer<Deserializer, json::ArrayRef> {
        using Base = serde::detail::IndexedArrayDeserializer<Deserializer, json::ArrayRef>;
        friend class Deserializer;

        DeserializeArray(Deserializer& deserializer,
                         json::ArrayRef array,
                         std::size_t expectedLength,
                         bool isStrictLength) :
            Base(deserializer, array, array.size(), expectedLength, isStrictLength) {}
    };

    class DeserializeObject :
        public serde::detail::IndexedObjectDeserializer<Deserializer, json::ValueRef> {
        using Base = serde::detail::IndexedObjectDeserializer<Deserializer, json::ValueRef>;
        friend class Deserializer;

        DeserializeObject(Deserializer& deserializer, json::ObjectRef object) : Base(deserializer) {
            auto collected = deserializer.collect_object_entries(object);
            if(!collected) {
                deserializer.mark_invalid(collected.error());
                return;
            }
            entries = std::move(*collected);
        }
    };

    using DeserializeSeq = DeserializeArray;
    using DeserializeTuple = DeserializeArray;
    using DeserializeMap = DeserializeObject;
    using DeserializeStruct = DeserializeObject;

    explicit Deserializer(const json::Value& value) : rootValue(value.as_ref()) {
        if(!rootValue.valid()) {
            mark_invalid();
        }
    }

    explicit Deserializer(json::ValueRef value) : rootValue(value) {
        if(!rootValue.valid()) {
            mark_invalid();
        }
    }

    [[nodiscard]] bool valid() const noexcept {
        return isValid;
    }

    [[nodiscard]] error_type error() const noexcept {
        return current_error();
    }

    status_t finish() {
        if(!isValid) {
            return std::unexpected(current_error());
        }
        if(!rootConsumed) {
            mark_invalid();
            return std::unexpected(current_error());
        }
        return {};
    }

    result_t<bool> deserialize_none() {
        if(!isValid) {
            return std::unexpected(current_error());
        }

        auto ref = peek_value_ref();
        if(!ref) {
            return std::unexpected(ref.error());
        }

        const bool isNone = ref->is_null();
        if(isNone && !hasCurrentValue) {
            rootConsumed = true;
        }
        return isNone;
    }

    template <typename... Ts>
    status_t deserialize_variant(std::variant<Ts...>& value) {
        auto valueKind = peek_value_kind();
        if(!valueKind) {
            return std::unexpected(valueKind.error());
        }

        auto source = consume_value_ref();
        if(!source) {
            return std::unexpected(source.error());
        }

        auto result = serde::try_variant_dispatch<Deserializer>(*source,
                                                                map_to_type_hint(*valueKind),
                                                                value,
                                                                error_type::type_mismatch);
        if(!result) {
            mark_invalid(result.error());
            return std::unexpected(current_error());
        }
        return {};
    }

    status_t deserialize_bool(bool& value) {
        return read_scalar(value, [](json::ValueRef ref) -> result_t<bool> {
            auto parsed = ref.get_bool();
            if(!parsed) {
                return std::unexpected(error_type::type_mismatch);
            }
            return *parsed;
        });
    }

    template <serde::int_like T>
    status_t deserialize_int(T& value) {
        std::int64_t parsed = 0;
        auto status = read_scalar(parsed, [](json::ValueRef ref) -> result_t<std::int64_t> {
            auto parsed = ref.get_int();
            if(!parsed) {
                return std::unexpected(error_type::type_mismatch);
            }
            return *parsed;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        auto narrowed = serde::detail::narrow_int<T>(parsed, error_type::number_out_of_range);
        if(!narrowed) {
            mark_invalid(narrowed.error());
            return std::unexpected(current_error());
        }

        value = *narrowed;
        return {};
    }

    template <serde::uint_like T>
    status_t deserialize_uint(T& value) {
        std::uint64_t parsed = 0;
        auto status = read_scalar(parsed, [](json::ValueRef ref) -> result_t<std::uint64_t> {
            auto parsed = ref.get_uint();
            if(!parsed) {
                return std::unexpected(error_type::type_mismatch);
            }
            return *parsed;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        auto narrowed = serde::detail::narrow_uint<T>(parsed, error_type::number_out_of_range);
        if(!narrowed) {
            mark_invalid(narrowed.error());
            return std::unexpected(current_error());
        }

        value = *narrowed;
        return {};
    }

    template <serde::floating_like T>
    status_t deserialize_float(T& value) {
        double parsed = 0.0;
        auto status = read_scalar(parsed, [](json::ValueRef ref) -> result_t<double> {
            auto parsed = ref.get_double();
            if(!parsed) {
                return std::unexpected(error_type::type_mismatch);
            }
            return *parsed;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        auto narrowed = serde::detail::narrow_float<T>(parsed, error_type::number_out_of_range);
        if(!narrowed) {
            mark_invalid(narrowed.error());
            return std::unexpected(current_error());
        }

        value = *narrowed;
        return {};
    }

    status_t deserialize_char(char& value) {
        std::string_view text;
        auto status = read_scalar(text, [](json::ValueRef ref) -> result_t<std::string_view> {
            auto parsed = ref.get_string();
            if(!parsed) {
                return std::unexpected(error_type::type_mismatch);
            }
            return *parsed;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        auto narrowed = serde::detail::narrow_char(text, error_type::type_mismatch);
        if(!narrowed) {
            mark_invalid(narrowed.error());
            return std::unexpected(current_error());
        }

        value = *narrowed;
        return {};
    }

    status_t deserialize_str(std::string& value) {
        std::string_view text;
        auto status = read_scalar(text, [](json::ValueRef ref) -> result_t<std::string_view> {
            auto parsed = ref.get_string();
            if(!parsed) {
                return std::unexpected(error_type::type_mismatch);
            }
            return *parsed;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        value.assign(text.data(), text.size());
        return {};
    }

    status_t deserialize_bytes(std::vector<std::byte>& value) {
        return serde::detail::deserialize_bytes_from_seq(*this, value);
    }

    result_t<DeserializeSeq> deserialize_seq(std::optional<std::size_t> len) {
        auto array = open_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        return DeserializeSeq(*this, *array, len.value_or(0), false);
    }

    result_t<DeserializeTuple> deserialize_tuple(std::size_t len) {
        auto array = open_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        return DeserializeTuple(*this, *array, len, true);
    }

    result_t<DeserializeMap> deserialize_map(std::optional<std::size_t> /*len*/) {
        auto object = open_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        DeserializeMap map(*this, *object);
        if(!isValid) {
            return std::unexpected(current_error());
        }
        return map;
    }

    result_t<DeserializeStruct> deserialize_struct(std::string_view /*name*/, std::size_t /*len*/) {
        auto object = open_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        DeserializeStruct structure(*this, *object);
        if(!isValid) {
            return std::unexpected(current_error());
        }
        return structure;
    }

    result_t<json::Value> capture_dom_value() {
        auto source = consume_value_ref();
        if(!source) {
            return std::unexpected(source.error());
        } else {
            auto copied = json::Value::copy_of(*source);
            if(!copied.has_value()) {
                mark_invalid(copied.error());
                return std::unexpected(current_error());
            } else {
                return std::move(*copied);
            }
        }
    }

private:
    friend class serde::detail::IndexedArrayDeserializer<Deserializer, json::ArrayRef>;
    friend class serde::detail::IndexedObjectDeserializer<Deserializer, json::ValueRef>;

    enum class value_kind : std::uint8_t {
        null,
        boolean,
        number,
        string,
        array,
        object,
    };

    template <typename T, typename Reader>
    status_t read_scalar(T& out, Reader&& reader) {
        if(!isValid) {
            return std::unexpected(current_error());
        }

        auto ref = peek_value_ref();
        if(!ref) {
            return std::unexpected(ref.error());
        }

        auto parsed = std::forward<Reader>(reader)(*ref);
        if(!parsed) {
            mark_invalid(parsed.error());
            return std::unexpected(current_error());
        }

        out = *parsed;
        if(!hasCurrentValue) {
            rootConsumed = true;
        }
        return {};
    }

    template <typename T>
    status_t deserialize_from_value_ref(json::ValueRef input, T& out) {
        struct value_scope {
            value_scope(Deserializer& deserializer, json::ValueRef input) :
                deserializer(deserializer), previousHasCurrentValue(deserializer.hasCurrentValue),
                previousCurrentValue(deserializer.currentValue) {
                deserializer.currentValue = input;
                deserializer.hasCurrentValue = true;
            }

            ~value_scope() {
                deserializer.currentValue = previousCurrentValue;
                deserializer.hasCurrentValue = previousHasCurrentValue;
            }

            Deserializer& deserializer;
            bool previousHasCurrentValue;
            json::ValueRef previousCurrentValue;
        };

        value_scope scope(*this, input);
        return serde::deserialize(*this, out);
    }

    /// Bridge methods for shared deserialize helpers.
    template <typename T>
    status_t deserialize_element_value(json::ArrayRef array, std::size_t index, T& out) {
        return deserialize_from_value_ref(array[index], out);
    }

    template <typename T>
    status_t deserialize_entry_value(json::ValueRef value, T& out) {
        return deserialize_from_value_ref(value, out);
    }

    result_t<value_kind> peek_value_kind() {
        auto ref = peek_value_ref();
        if(!ref) {
            return std::unexpected(ref.error());
        }

        if(ref->is_null()) {
            return value_kind::null;
        }
        if(ref->is_bool()) {
            return value_kind::boolean;
        }
        if(ref->is_number()) {
            return value_kind::number;
        }
        if(ref->is_string()) {
            return value_kind::string;
        }
        if(ref->is_array()) {
            return value_kind::array;
        }
        if(ref->is_object()) {
            return value_kind::object;
        }

        mark_invalid(error_type::type_mismatch);
        return std::unexpected(current_error());
    }

    static serde::type_hint map_to_type_hint(value_kind kind) {
        switch(kind) {
            case value_kind::null: return serde::type_hint::null_like;
            case value_kind::boolean: return serde::type_hint::boolean;
            case value_kind::number: return serde::type_hint::integer | serde::type_hint::floating;
            case value_kind::string: return serde::type_hint::string;
            case value_kind::array: return serde::type_hint::array;
            case value_kind::object: return serde::type_hint::object;
            default: return serde::type_hint::any;
        }
    }

    result_t<std::vector<typename DeserializeObject::entry>>
        collect_object_entries(json::ObjectRef object) {
        std::vector<typename DeserializeObject::entry> entries;
        entries.reserve(object.size());

        for(auto entry: object) {
            entries.push_back(typename DeserializeObject::entry{
                .key = entry.key,
                .value = entry.value,
            });
        }

        return entries;
    }

    result_t<json::ValueRef> access_value_ref(bool consume) {
        if(!isValid) {
            return std::unexpected(current_error());
        }
        if(hasCurrentValue) {
            return currentValue;
        }
        if(rootConsumed || !rootValue.valid()) {
            mark_invalid();
            return std::unexpected(current_error());
        }
        if(consume) {
            rootConsumed = true;
        }
        return rootValue;
    }

    result_t<json::ValueRef> peek_value_ref() {
        return access_value_ref(false);
    }

    result_t<json::ValueRef> consume_value_ref() {
        return access_value_ref(true);
    }

    result_t<json::ArrayRef> open_array() {
        auto ref = consume_value_ref();
        if(!ref) {
            return std::unexpected(ref.error());
        }

        auto array = ref->get_array();
        if(!array) {
            mark_invalid(error_type::type_mismatch);
            return std::unexpected(current_error());
        }
        return *array;
    }

    result_t<json::ObjectRef> open_object() {
        auto ref = consume_value_ref();
        if(!ref) {
            return std::unexpected(ref.error());
        }

        auto object = ref->get_object();
        if(!object) {
            mark_invalid(error_type::type_mismatch);
            return std::unexpected(current_error());
        }
        return std::move(*object);
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
    bool rootConsumed = false;
    error_type lastError = error_type::invalid_state;
    json::ValueRef rootValue{};
    bool hasCurrentValue = false;
    json::ValueRef currentValue{};
};

static_assert(serde::deserializer_like<Deserializer>);

}  // namespace eventide::serde::json::yy
