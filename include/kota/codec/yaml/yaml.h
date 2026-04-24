#pragma once

#include <concepts>
#include <expected>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "kota/codec/yaml/deserializer.h"
#include "kota/codec/yaml/error.h"
#include "kota/codec/yaml/serializer.h"

namespace kota::codec::yaml {

inline auto parse_node(std::string_view text) -> std::expected<YAML::Node, error> {
    try {
        return YAML::Load(std::string(text));
    } catch(const YAML::ParserException&) {
        return std::unexpected(error_kind::parse_error);
    }
}

template <typename T>
auto parse(std::string_view text, T& value) -> std::expected<void, error> {
    auto node = parse_node(text);
    if(!node) {
        return std::unexpected(node.error());
    }
    return from_yaml(*node, value);
}

template <typename T>
    requires std::default_initializable<T>
auto parse(std::string_view text) -> std::expected<T, error> {
    auto node = parse_node(text);
    if(!node) {
        return std::unexpected(node.error());
    }
    return from_yaml<T>(*node);
}

template <typename T>
auto to_string(const T& value) -> std::expected<std::string, error> {
    auto node = to_yaml(value);
    if(!node) {
        return std::unexpected(node.error());
    }

    YAML::Emitter emitter;
    emitter << *node;
    return std::string(emitter.c_str());
}

}  // namespace kota::codec::yaml

namespace kota::codec {

template <typename Config>
struct deserialize_traits<yaml::Deserializer<Config>, YAML::Node> {
    using error_type = yaml::Deserializer<Config>::error_type;

    static auto deserialize(yaml::Deserializer<Config>& deserializer, YAML::Node& value)
        -> std::expected<void, error_type> {
        auto captured = deserializer.capture_dom_value();
        if(!captured) {
            return std::unexpected(captured.error());
        }
        value = std::move(*captured);
        return {};
    }
};

}  // namespace kota::codec
