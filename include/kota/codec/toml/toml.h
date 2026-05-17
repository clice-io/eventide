#pragma once

#include <concepts>
#include <expected>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "kota/codec/toml/decode.h"
#include "kota/codec/toml/encode.h"
#include "kota/codec/toml/type.h"

namespace kota::codec::toml {

template <typename T>
auto parse(std::string_view text, T& value) -> std::expected<void, rich_error> {
    return from_toml(text, value);
}

template <typename T>
    requires std::default_initializable<T>
auto parse(std::string_view text) -> std::expected<T, rich_error> {
    T value{};
    auto result = from_toml(text, value);
    if(!result) {
        return std::unexpected(std::move(result).error());
    }
    return value;
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
struct deserialize_visit<toml::value_reader, ::toml::table, Config> {
    static bool visit(toml::value_reader& vis, ::toml::table& value) {
        if(!vis.node) {
            return scoped_context<rich_error>::fail(rich_error::invalid_type("table", "null"));
        }
        const auto* tbl = vis.node->as_table();
        if(!tbl) {
            return scoped_context<rich_error>::fail(
                rich_error::invalid_type("table", toml::detail::toml_node_type_name(vis.node)));
        }
        value = *tbl;
        return true;
    }
};

template <typename Config>
struct deserialize_visit<toml::value_reader, ::toml::array, Config> {
    static bool visit(toml::value_reader& vis, ::toml::array& value) {
        if(!vis.node) {
            return scoped_context<rich_error>::fail(rich_error::invalid_type("array", "null"));
        }
        const auto* arr = vis.node->as_array();
        if(!arr) {
            return scoped_context<rich_error>::fail(
                rich_error::invalid_type("array", toml::detail::toml_node_type_name(vis.node)));
        }
        value = *arr;
        return true;
    }
};

}  // namespace kota::codec
