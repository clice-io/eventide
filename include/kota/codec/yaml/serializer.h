#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "kota/support/expected_try.h"
#include "kota/codec/detail/backend.h"
#include "kota/codec/detail/codec.h"
#include "kota/codec/detail/config.h"
#include "kota/codec/yaml/error.h"

#if __has_include(<yaml-cpp/yaml.h>)
#include "yaml-cpp/yaml.h"
#else
#error "yaml-cpp/yaml.h not found. Enable KOTA_CODEC_ENABLE_YAML or add yaml-cpp include paths."
#endif

namespace kota::codec::yaml {

template <typename Config = config::default_config>
class Serializer {
public:
    using config_type = Config;
    using value_type = void;
    using error_type = error_kind;

    constexpr static auto backend_kind_v = backend_kind::streaming;
    constexpr static auto field_mode_v = field_mode::by_name;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    status_t serialize_null() {
        return insert_value(YAML::Node(YAML::NodeType::Null));
    }

    template <typename T>
    status_t serialize_some(const T& value) {
        return codec::serialize(*this, value);
    }

    template <typename... Ts>
    status_t serialize_variant(const std::variant<Ts...>& value) {
        return std::visit(
            [&](const auto& item) -> status_t { return codec::serialize(*this, item); }, value);
    }

    status_t serialize_bool(bool value) {
        return insert_value(YAML::Node(value));
    }

    status_t serialize_int(std::int64_t value) {
        return insert_value(YAML::Node(value));
    }

    status_t serialize_uint(std::uint64_t value) {
        return insert_value(YAML::Node(value));
    }

    status_t serialize_float(double value) {
        return insert_value(YAML::Node(value));
    }

    status_t serialize_char(char value) {
        return insert_value(YAML::Node(std::string(1, value)));
    }

    status_t serialize_str(std::string_view value) {
        return insert_value(YAML::Node(std::string(value)));
    }

    status_t serialize_bytes(std::span<const std::byte> value) {
        KOTA_EXPECTED_TRY(begin_array(value.size()));
        for(const auto byte: value) {
            KOTA_EXPECTED_TRY(insert_value(
                YAML::Node(static_cast<int>(std::to_integer<std::uint8_t>(byte)))));
        }
        return end_array();
    }

    status_t begin_object(std::size_t /*count*/) {
        ser_frame frame;
        frame.node = YAML::Node(YAML::NodeType::Map);
        ser_stack.push_back(std::move(frame));
        return {};
    }

    status_t field(std::string_view name) {
        ser_stack.back().pending_key = std::string(name);
        return {};
    }

    status_t end_object() {
        auto frame = std::move(ser_stack.back());
        ser_stack.pop_back();

        if(ser_stack.empty()) {
            root_ = frame.node;
            return {};
        }

        auto& parent = ser_stack.back();
        if(parent.is_array) {
            parent.node.push_back(frame.node);
        } else {
            parent.node[parent.pending_key] = frame.node;
            parent.pending_key.clear();
        }
        return {};
    }

    template <typename F>
    status_t serialize_field(std::string_view name, F&& writer) {
        KOTA_EXPECTED_TRY(field(name));
        return std::forward<F>(writer)();
    }

    template <typename F>
    status_t serialize_element(F&& writer) {
        return std::forward<F>(writer)();
    }

    status_t begin_array(std::optional<std::size_t> /*count*/) {
        ser_frame frame;
        frame.node = YAML::Node(YAML::NodeType::Sequence);
        frame.is_array = true;
        ser_stack.push_back(std::move(frame));
        return {};
    }

    status_t end_array() {
        auto frame = std::move(ser_stack.back());
        ser_stack.pop_back();

        if(ser_stack.empty()) {
            root_ = frame.node;
            return {};
        }

        auto& parent = ser_stack.back();
        if(parent.is_array) {
            parent.node.push_back(frame.node);
        } else {
            parent.node[parent.pending_key] = frame.node;
            parent.pending_key.clear();
        }
        return {};
    }

    template <typename T>
    auto dom(const T& value) -> result_t<YAML::Node> {
        root_.reset();
        ser_stack.clear();
        auto status = codec::serialize(*this, value);
        if(!status) {
            root_.reset();
            ser_stack.clear();
            return std::unexpected(status.error());
        }
        return std::move(root_);
    }

private:
    status_t insert_value(YAML::Node value_node) {
        if(ser_stack.empty()) {
            root_ = std::move(value_node);
            return {};
        }
        auto& frame = ser_stack.back();
        if(frame.is_array) {
            frame.node.push_back(std::move(value_node));
        } else {
            frame.node[frame.pending_key] = std::move(value_node);
            frame.pending_key.clear();
        }
        return {};
    }

    struct ser_frame {
        YAML::Node node;
        std::string pending_key;
        bool is_array = false;
    };

    YAML::Node root_;
    std::vector<ser_frame> ser_stack;
};

template <typename Config = config::default_config, typename T>
auto to_yaml(const T& value) -> std::expected<YAML::Node, error> {
    Serializer<Config> serializer;
    auto result = serializer.dom(value);
    if(!result) {
        return std::unexpected(result.error());
    }
    return std::move(*result);
}

static_assert(codec::serializer_like<Serializer<>>);

}  // namespace kota::codec::yaml
