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

template <typename Sink, typename Config>
struct serialize_visit<toml::ValueWriter<Sink>, toml::Table, Config> {
    static bool visit(toml::ValueWriter<Sink>& vis, const toml::Table& value) {
        return vis.sink.emit(value);
    }
};

template <typename Sink, typename Config>
struct serialize_visit<toml::ValueWriter<Sink>, toml::Array, Config> {
    static bool visit(toml::ValueWriter<Sink>& vis, const toml::Array& value) {
        return vis.sink.emit(value);
    }
};

template <typename Config>
struct deserialize_visit<toml::ValueReader, toml::Table, Config> {
    static bool visit(toml::ValueReader& vis, toml::Table& value) {
        if(!vis.node) {
            return scoped_context<rich_error>::fail(rich_error::invalid_type("table", "null"));
        }
        const auto* tbl = vis.node->as_table();
        if(!tbl) {
            return scoped_context<rich_error>::fail(
                rich_error::invalid_type("table", toml::detail::node_type_name(vis.node)));
        }
        value = *tbl;
        return true;
    }
};

template <typename Config>
struct deserialize_visit<toml::ValueReader, toml::Array, Config> {
    static bool visit(toml::ValueReader& vis, toml::Array& value) {
        if(!vis.node) {
            return scoped_context<rich_error>::fail(rich_error::invalid_type("array", "null"));
        }
        const auto* arr = vis.node->as_array();
        if(!arr) {
            return scoped_context<rich_error>::fail(
                rich_error::invalid_type("array", toml::detail::node_type_name(vis.node)));
        }
        value = *arr;
        return true;
    }
};

}  // namespace kota::codec
