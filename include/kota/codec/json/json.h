#pragma once

#include <concepts>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "kota/codec/dyn/document.h"
#include "kota/codec/dyn/encode.h"
#include "kota/codec/json/decode.h"
#include "kota/codec/json/encode.h"
#include "kota/codec/json/type.h"
#include "kota/codec/visit/raw_value.h"

namespace kota::codec::json {

// DOM type aliases (shared with content backend)
using ValueKind = dyn::ValueKind;
using Cursor = dyn::Cursor;
using Value = dyn::Value;
using Array = dyn::Array;
using Object = dyn::Object;

// Top-level convenience API (uses streaming simdjson backend by default)

template <typename Config = void, typename T>
auto parse(std::string_view json, T& value) -> std::expected<void, rich_error> {
    return from_json<Config>(json, value);
}

template <typename T, typename Config = void>
    requires std::default_initializable<T>
auto parse(std::string_view json) -> std::expected<T, rich_error> {
    return from_json<Config, T>(json);
}

template <typename Config = void, typename T>
auto to_string(const T& value, std::optional<std::size_t> initial_capacity = std::nullopt)
    -> std::expected<std::string, error> {
    return to_json<Config>(value, initial_capacity);
}

inline std::expected<std::string, error> prettify(std::string_view json) {
    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    auto padded = simdjson::padded_string(json);
    if(auto err = parser.parse(padded).get(doc)) {
        return std::unexpected(rich_error(std::string(error_message(make_error(err)))));
    }
    return simdjson::prettify(doc);
}

}  // namespace kota::codec::json

namespace kota::codec {

template <typename Config>
struct serialize_visit<json::value_writer, RawValue, Config> {
    static bool visit(json::value_writer& vis, const RawValue& value) {
        if(value.empty()) {
            return vis.visit_null();
        }
        vis.builder.append_raw(value.data);
        return true;
    }
};

template <typename Config>
struct deserialize_visit<json::reader, RawValue, Config> {
    static bool visit(json::reader& vis, RawValue& value) {
        simdjson::ondemand::json_type t;
        if(vis.apply([&](auto& s) { return s.type().get(t); }) != simdjson::SUCCESS)
            return false;
        std::string_view raw;
        if(vis.apply([&](auto& s) { return s.raw_json().get(raw); }) != simdjson::SUCCESS)
            return false;
        value.data.assign(raw.data(), raw.size());
        return true;
    }
};

namespace detail {

inline bool decode_dyn_value_from_json(json::reader& vis, dyn::Value& value) {
    simdjson::ondemand::json_type t;
    if(vis.apply([&](auto& s) { return s.type().get(t); }) != simdjson::SUCCESS)
        return false;

    switch(t) {
        case simdjson::ondemand::json_type::null:
            if(!vis.visit_null())
                return false;
            value = dyn::Value(nullptr);
            return true;
        case simdjson::ondemand::json_type::boolean: {
            bool b = false;
            if(!vis.visit_bool(b))
                return false;
            value = dyn::Value(b);
            return true;
        }
        case simdjson::ondemand::json_type::number: {
            std::int64_t i = 0;
            if(vis.apply([&](auto& s) { return s.get_int64().get(i); }) == simdjson::SUCCESS) {
                value = dyn::Value(i);
                return true;
            }
            std::uint64_t u = 0;
            if(vis.apply([&](auto& s) { return s.get_uint64().get(u); }) == simdjson::SUCCESS) {
                value = dyn::Value(u);
                return true;
            }
            double d = 0.0;
            if(!vis.visit_float(d))
                return false;
            value = dyn::Value(d);
            return true;
        }
        case simdjson::ondemand::json_type::string: {
            std::string s;
            if(!vis.visit_str(s))
                return false;
            value = dyn::Value(std::move(s));
            return true;
        }
        case simdjson::ondemand::json_type::array: {
            dyn::Array arr;
            bool ok = vis.visit_seq([&](auto& ev) -> bool {
                dyn::Value elem;
                if(!decode_dyn_value_from_json(ev, elem))
                    return false;
                arr.push_back(std::move(elem));
                return true;
            });
            if(!ok)
                return false;
            value = dyn::Value(std::move(arr));
            return true;
        }
        case simdjson::ondemand::json_type::object: {
            dyn::Object obj;
            bool ok = vis.visit_struct([&](std::string_view key, json::reader& fv) -> bool {
                dyn::Value field_value;
                if(!decode_dyn_value_from_json(fv, field_value))
                    return false;
                obj.insert(std::string(key), std::move(field_value));
                return true;
            });
            if(!ok)
                return false;
            value = dyn::Value(std::move(obj));
            return true;
        }
        default: break;
    }
    return false;
}

}  // namespace detail

template <typename Config>
struct deserialize_visit<json::reader, dyn::Value, Config> {
    static bool visit(json::reader& vis, dyn::Value& value) {
        return detail::decode_dyn_value_from_json(vis, value);
    }
};

}  // namespace kota::codec
