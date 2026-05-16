#pragma once

#include <concepts>
#include <expected>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "kota/codec/toml/deserializer.h"
#include "kota/codec/toml/encode.h"
#include "kota/codec/toml/type.h"

namespace kota::codec::toml {

inline auto parse_table(std::string_view text) -> std::expected<::toml::table, error> {
#if TOML_EXCEPTIONS
    try {
        return ::toml::parse(text);
    } catch(const ::toml::parse_error&) {
        return std::unexpected(error_kind::parse_error);
    }
#else
    auto parsed = ::toml::parse(text);
    if(!parsed) {
        return std::unexpected(error_kind::parse_error);
    }
    return std::move(parsed).table();
#endif
}

template <typename T>
auto parse(std::string_view text, T& value) -> std::expected<void, error> {
    auto table = parse_table(text);
    if(!table) {
        return std::unexpected(table.error());
    }
    return from_toml(*table, value);
}

template <typename T>
    requires std::default_initializable<T>
auto parse(std::string_view text) -> std::expected<T, error> {
    auto table = parse_table(text);
    if(!table) {
        return std::unexpected(table.error());
    }
    return from_toml<T>(*table);
}

template <typename T>
auto to_string(const T& value) -> std::expected<std::string, error> {
    auto table = to_toml(value);
    if(!table) {
        return std::unexpected(table.error());
    }

    std::ostringstream out;
    out << *table;
    return out.str();
}

}  // namespace kota::codec::toml

namespace kota::codec {

template <typename Config>
struct serialize_visit<toml::table_value_writer, ::toml::table, Config> {
    static bool visit(toml::table_value_writer& vis, const ::toml::table& value) {
        vis.parent_table.insert_or_assign(vis.key, value);
        return true;
    }
};

template <typename Config>
struct serialize_visit<toml::table_value_writer, ::toml::array, Config> {
    static bool visit(toml::table_value_writer& vis, const ::toml::array& value) {
        vis.parent_table.insert_or_assign(vis.key, value);
        return true;
    }
};

template <typename Config>
struct serialize_visit<toml::array_value_writer, ::toml::table, Config> {
    static bool visit(toml::array_value_writer& vis, const ::toml::table& value) {
        vis.arr.push_back(value);
        return true;
    }
};

template <typename Config>
struct serialize_visit<toml::array_value_writer, ::toml::array, Config> {
    static bool visit(toml::array_value_writer& vis, const ::toml::array& value) {
        vis.arr.push_back(value);
        return true;
    }
};

template <typename Config>
struct deserialize_traits<toml::Deserializer<Config>, ::toml::table> {
    using error_type = toml::Deserializer<Config>::error_type;

    static auto deserialize(toml::Deserializer<Config>& deserializer, ::toml::table& value)
        -> std::expected<void, error_type> {
        auto table = deserializer.capture_table();
        if(!table) {
            return std::unexpected(table.error());
        }
        value = std::move(*table);
        return {};
    }
};

template <typename Config>
struct deserialize_traits<toml::Deserializer<Config>, ::toml::array> {
    using error_type = toml::Deserializer<Config>::error_type;

    static auto deserialize(toml::Deserializer<Config>& deserializer, ::toml::array& value)
        -> std::expected<void, error_type> {
        auto array = deserializer.capture_array();
        if(!array) {
            return std::unexpected(array.error());
        }
        value = std::move(*array);
        return {};
    }
};

}  // namespace kota::codec
