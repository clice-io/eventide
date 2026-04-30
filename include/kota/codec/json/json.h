#pragma once

#include <concepts>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "kota/codec/content/deserializer.h"
#include "kota/codec/content/document.h"
#include "kota/codec/content/serializer.h"
#include "kota/codec/detail/config.h"
#include "kota/codec/detail/raw_value.h"
#include "kota/codec/json/deserializer.h"
#include "kota/codec/json/error.h"
#include "kota/codec/json/serializer.h"

namespace kota::codec::json {

// DOM type aliases (shared with content backend)
using ValueKind = content::ValueKind;
using Cursor = content::Cursor;
using Value = content::Value;
using Array = content::Array;
using Object = content::Object;

// Top-level convenience API (uses streaming simdjson backend by default)

template <typename Config = config::default_config, typename T>
auto parse(std::string_view json, T& value) -> std::expected<void, error> {
    return from_json<Config>(json, value);
}

template <typename T, typename Config = config::default_config>
    requires std::default_initializable<T>
auto parse(std::string_view json) -> std::expected<T, error> {
    return from_json<T, Config>(json);
}

template <typename Config = config::default_config, typename T>
auto to_string(const T& value, std::optional<std::size_t> initial_capacity = std::nullopt)
    -> std::expected<std::string, error> {
    return to_json<Config>(value, initial_capacity);
}

}  // namespace kota::codec::json

namespace kota::codec {

template <typename Config>
struct deserialize_traits<json::Deserializer<Config>, content::Value> {
    using D = json::Deserializer<Config>;
    using error_type = json::error;

    static auto deserialize(D& d, content::Value& value) -> std::expected<void, error_type> {
        KOTA_EXPECTED_TRY_V(auto type, d.peek_type());
        switch(type) {
            case simdjson::ondemand::json_type::null: {
                KOTA_EXPECTED_TRY_V(auto is_none, d.deserialize_none());
                (void)is_none;
                value = content::Value(nullptr);
                return {};
            }
            case simdjson::ondemand::json_type::boolean: {
                bool b = false;
                KOTA_EXPECTED_TRY(d.deserialize_bool(b));
                value = content::Value(b);
                return {};
            }
            case simdjson::ondemand::json_type::number: {
                KOTA_EXPECTED_TRY_V(auto nt, d.peek_number_type());
                if(nt == simdjson::ondemand::number_type::floating_point_number) {
                    double f = 0.0;
                    KOTA_EXPECTED_TRY(d.deserialize_float(f));
                    value = content::Value(f);
                } else if(nt == simdjson::ondemand::number_type::signed_integer) {
                    std::int64_t i = 0;
                    KOTA_EXPECTED_TRY(d.deserialize_int(i));
                    value = content::Value(i);
                } else {
                    std::uint64_t u = 0;
                    KOTA_EXPECTED_TRY(d.deserialize_uint(u));
                    value = content::Value(u);
                }
                return {};
            }
            case simdjson::ondemand::json_type::string: {
                std::string s;
                KOTA_EXPECTED_TRY(d.deserialize_str(s));
                value = content::Value(std::move(s));
                return {};
            }
            case simdjson::ondemand::json_type::array: {
                content::Array arr;
                KOTA_EXPECTED_TRY(d.begin_array());
                while(true) {
                    KOTA_EXPECTED_TRY_V(auto has, d.next_element());
                    if(!has)
                        break;
                    content::Value elem;
                    KOTA_EXPECTED_TRY(codec::deserialize(d, elem));
                    arr.push_back(std::move(elem));
                }
                KOTA_EXPECTED_TRY(d.end_array());
                value = content::Value(std::move(arr));
                return {};
            }
            case simdjson::ondemand::json_type::object: {
                content::Object obj;
                KOTA_EXPECTED_TRY(d.begin_object());
                while(true) {
                    KOTA_EXPECTED_TRY_V(auto key, d.next_field());
                    if(!key.has_value())
                        break;
                    std::string key_copy(*key);
                    content::Value field_value;
                    KOTA_EXPECTED_TRY(codec::deserialize(d, field_value));
                    obj.insert(std::move(key_copy), std::move(field_value));
                }
                KOTA_EXPECTED_TRY(d.end_object());
                value = content::Value(std::move(obj));
                return {};
            }
            default: return std::unexpected(json::error_kind::type_mismatch);
        }
    }
};

template <typename Config>
struct deserialize_traits<json::Deserializer<Config>, content::Array> {
    using D = json::Deserializer<Config>;
    using error_type = json::error;

    static auto deserialize(D& d, content::Array& value) -> std::expected<void, error_type> {
        content::Value v;
        KOTA_EXPECTED_TRY((deserialize_traits<D, content::Value>::deserialize(d, v)));
        auto* arr = v.get_array();
        if(arr == nullptr) {
            return std::unexpected(json::error_kind::type_mismatch);
        }
        value = std::move(*arr);
        return {};
    }
};

template <typename Config>
struct deserialize_traits<json::Deserializer<Config>, content::Object> {
    using D = json::Deserializer<Config>;
    using error_type = json::error;

    static auto deserialize(D& d, content::Object& value) -> std::expected<void, error_type> {
        content::Value v;
        KOTA_EXPECTED_TRY((deserialize_traits<D, content::Value>::deserialize(d, v)));
        auto* obj = v.get_object();
        if(obj == nullptr) {
            return std::unexpected(json::error_kind::type_mismatch);
        }
        value = std::move(*obj);
        return {};
    }
};

template <typename Config>
struct serialize_traits<json::Serializer<Config>, RawValue> {
    using value_type = typename json::Serializer<Config>::value_type;
    using error_type = typename json::Serializer<Config>::error_type;

    static auto serialize(json::Serializer<Config>& serializer, const RawValue& value)
        -> std::expected<value_type, error_type> {
        if(value.empty()) {
            return serializer.serialize_null();
        }
        return serializer.serialize_raw_json(value.data);
    }
};

template <typename Config>
struct deserialize_traits<json::Deserializer<Config>, RawValue> {
    using error_type = typename json::Deserializer<Config>::error_type;

    static auto deserialize(json::Deserializer<Config>& deserializer, RawValue& value)
        -> std::expected<void, error_type> {
        auto raw = deserializer.deserialize_raw_json_view();
        if(!raw) {
            return std::unexpected(raw.error());
        }
        value.data.assign(raw->data(), raw->size());
        return {};
    }
};

}  // namespace kota::codec
