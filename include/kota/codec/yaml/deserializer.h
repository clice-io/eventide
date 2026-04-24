#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "kota/support/expected_try.h"
#include "kota/codec/detail/backend.h"
#include "kota/codec/detail/codec.h"
#include "kota/codec/detail/common.h"
#include "kota/codec/detail/config.h"
#include "kota/codec/detail/narrow.h"
#include "kota/codec/yaml/error.h"

#if __has_include(<yaml-cpp/yaml.h>)
#include "yaml-cpp/yaml.h"
#else
#error "yaml-cpp/yaml.h not found. Enable KOTA_CODEC_ENABLE_YAML or add yaml-cpp include paths."
#endif

namespace kota::codec::yaml {

// YAML::Node::operator= calls set_data() which mutates the underlying shared
// data instead of rebinding. This helper always copy-constructs a fresh Node
// via optional::emplace to avoid corrupting aliased nodes.
class node_slot {
public:
    void set(const YAML::Node& n) {
        slot.emplace(n);
    }

    void clear() {
        slot.reset();
    }

    [[nodiscard]] bool has_value() const noexcept {
        return slot.has_value();
    }

    YAML::Node& get() {
        return *slot;
    }

    const YAML::Node& get() const {
        return *slot;
    }

private:
    std::optional<YAML::Node> slot;
};

template <typename Config = config::default_config>
class Deserializer {
public:
    using config_type = Config;
    using error_type = yaml::error;

    constexpr static auto backend_kind_v = backend_kind::streaming;
    constexpr static auto field_mode_v = field_mode::by_name;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    explicit Deserializer(const YAML::Node& root) : root_node(root) {}

    [[nodiscard]] bool valid() const noexcept {
        return is_valid;
    }

    [[nodiscard]] error_type error() const noexcept {
        return last_error;
    }

    status_t finish() {
        if(!is_valid) {
            return std::unexpected(last_error);
        }
        if(!root_consumed) {
            return mark_invalid(error_kind::invalid_state);
        }
        return {};
    }

    result_t<bool> deserialize_none() {
        auto node = peek_node();
        if(!node) {
            return std::unexpected(node.error());
        }

        const bool is_none = !node->IsDefined() || node->IsNull();
        if(is_none && !current_value.has_value()) {
            root_consumed = true;
        }
        return is_none;
    }

    template <typename... Ts>
    status_t deserialize_variant(std::variant<Ts...>& value) {
        auto kind = peek_node_kind();
        if(!kind) {
            return std::unexpected(kind.error());
        }

        auto source = consume_node();
        if(!source) {
            return std::unexpected(source.error());
        }

        auto result = codec::try_variant_dispatch<Deserializer>(*source,
                                                                map_to_type_hint(*kind),
                                                                value,
                                                                error_type::type_mismatch);
        if(!result) {
            return mark_invalid(result.error());
        }
        return {};
    }

    status_t deserialize_bool(bool& value) {
        return read_scalar(value, [](const YAML::Node& node) -> result_t<bool> {
            bool v{};
            if(!YAML::convert<bool>::decode(node, v)) {
                return std::unexpected(error_kind::type_mismatch);
            }
            return v;
        });
    }

    template <codec::int_like T>
    status_t deserialize_int(T& value) {
        std::int64_t parsed = 0;
        auto status = read_scalar(parsed, [](const YAML::Node& node) -> result_t<std::int64_t> {
            std::int64_t v{};
            if(!YAML::convert<std::int64_t>::decode(node, v)) {
                return std::unexpected(error_kind::type_mismatch);
            }
            return v;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        auto narrowed = codec::detail::narrow_int<T>(parsed, error_kind::number_out_of_range);
        if(!narrowed) {
            return mark_invalid(narrowed.error());
        }

        value = *narrowed;
        return {};
    }

    template <codec::uint_like T>
    status_t deserialize_uint(T& value) {
        std::int64_t parsed = 0;
        auto status = read_scalar(parsed, [](const YAML::Node& node) -> result_t<std::int64_t> {
            std::int64_t v{};
            if(!YAML::convert<std::int64_t>::decode(node, v)) {
                return std::unexpected(error_kind::type_mismatch);
            }
            return v;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        if(parsed < 0) {
            return mark_invalid(error_kind::number_out_of_range);
        }

        const auto unsigned_value = static_cast<std::uint64_t>(parsed);
        auto narrowed =
            codec::detail::narrow_uint<T>(unsigned_value, error_kind::number_out_of_range);
        if(!narrowed) {
            return mark_invalid(narrowed.error());
        }

        value = *narrowed;
        return {};
    }

    template <codec::floating_like T>
    status_t deserialize_float(T& value) {
        double parsed = 0.0;
        auto status = read_scalar(parsed, [](const YAML::Node& node) -> result_t<double> {
            double v{};
            if(!YAML::convert<double>::decode(node, v)) {
                return std::unexpected(error_kind::type_mismatch);
            }
            return v;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        auto narrowed = codec::detail::narrow_float<T>(parsed, error_kind::number_out_of_range);
        if(!narrowed) {
            return mark_invalid(narrowed.error());
        }

        value = *narrowed;
        return {};
    }

    status_t deserialize_char(char& value) {
        std::string text;
        auto status = read_scalar(text, [](const YAML::Node& node) -> result_t<std::string> {
            std::string v;
            if(!YAML::convert<std::string>::decode(node, v)) {
                return std::unexpected(error_kind::type_mismatch);
            }
            return v;
        });
        if(!status) {
            return std::unexpected(status.error());
        }

        auto narrowed =
            codec::detail::narrow_char(std::string_view(text), error_kind::type_mismatch);
        if(!narrowed) {
            return mark_invalid(narrowed.error());
        }

        value = *narrowed;
        return {};
    }

    status_t deserialize_str(std::string& value) {
        return read_scalar(value, [](const YAML::Node& node) -> result_t<std::string> {
            std::string v;
            if(!YAML::convert<std::string>::decode(node, v)) {
                return std::unexpected(error_kind::type_mismatch);
            }
            return v;
        });
    }

    status_t deserialize_bytes(std::vector<std::byte>& value) {
        KOTA_EXPECTED_TRY(begin_array());
        value.clear();
        while(true) {
            KOTA_EXPECTED_TRY_V(auto has_next, next_element());
            if(!has_next) {
                break;
            }
            std::uint64_t byte_val = 0;
            KOTA_EXPECTED_TRY(deserialize_uint(byte_val));
            if(byte_val > 255U) {
                return mark_invalid(error_kind::number_out_of_range);
            }
            value.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(byte_val)));
        }
        return end_array();
    }

    result_t<YAML::Node> capture_dom_value() {
        auto node = consume_node();
        if(!node) {
            return std::unexpected(node.error());
        }
        return YAML::Clone(*node);
    }

    status_t begin_object() {
        auto node = consume_node();
        if(!node) {
            return std::unexpected(node.error());
        }
        if(!node->IsMap()) {
            return mark_invalid(error_kind::type_mismatch);
        }

        deser_frame frame;
        frame.node.set(*node);
        frame.iter = node->begin();
        frame.end_iter = node->end();
        deser_stack.push_back(std::move(frame));
        return {};
    }

    result_t<std::optional<std::string_view>> next_field() {
        if(!is_valid || deser_stack.empty()) {
            return mark_invalid(error_kind::invalid_state);
        }
        auto& frame = deser_stack.back();

        if(frame.pending_valid) {
            ++frame.iter;
            frame.pending_valid = false;
        }

        if(frame.iter == frame.end_iter) {
            current_value.clear();
            return std::optional<std::string_view>(std::nullopt);
        }

        YAML::convert<std::string>::decode(frame.iter->first, frame.pending_key);
        frame.pending_valid = true;
        current_value.set(frame.iter->second);
        return std::optional<std::string_view>(frame.pending_key);
    }

    status_t skip_field_value() {
        if(!is_valid || deser_stack.empty()) {
            return mark_invalid(error_kind::invalid_state);
        }
        auto& frame = deser_stack.back();
        ++frame.iter;
        frame.pending_valid = false;
        current_value.clear();
        return {};
    }

    status_t end_object() {
        if(!is_valid || deser_stack.empty()) {
            return mark_invalid(error_kind::invalid_state);
        }
        deser_stack.pop_back();
        current_value.clear();
        return {};
    }

    status_t begin_array() {
        auto node = consume_node();
        if(!node) {
            return std::unexpected(node.error());
        }
        if(!node->IsSequence()) {
            return mark_invalid(error_kind::type_mismatch);
        }
        array_frame frame;
        frame.node.set(*node);
        array_stack.push_back(std::move(frame));
        return {};
    }

    result_t<bool> next_element() {
        if(!is_valid || array_stack.empty()) {
            return mark_invalid(error_kind::invalid_state);
        }
        auto& frame = array_stack.back();
        if(frame.index >= frame.node.get().size()) {
            current_value.clear();
            return false;
        }
        current_value.set(frame.node.get()[frame.index]);
        ++frame.index;
        return true;
    }

    status_t end_array() {
        if(!is_valid || array_stack.empty()) {
            return mark_invalid(error_kind::invalid_state);
        }
        array_stack.pop_back();
        current_value.clear();
        return {};
    }

private:
    enum class node_kind : std::uint8_t {
        none,
        boolean,
        integer,
        floating,
        string,
        sequence,
        map,
        unknown,
    };

    template <typename T, typename Reader>
    status_t read_scalar(T& out, Reader&& reader) {
        auto node = consume_node();
        if(!node) {
            return std::unexpected(node.error());
        }
        if(!node->IsDefined() || node->IsNull()) {
            return mark_invalid(error_kind::type_mismatch);
        }

        auto parsed = std::forward<Reader>(reader)(*node);
        if(!parsed) {
            return mark_invalid(parsed.error());
        }

        out = std::move(*parsed);
        return {};
    }

    result_t<node_kind> peek_node_kind() {
        auto node = peek_node();
        if(!node) {
            return std::unexpected(node.error());
        }
        return classify_node(*node);
    }

    static auto classify_node(const YAML::Node& node) -> node_kind {
        if(!node.IsDefined() || node.IsNull()) {
            return node_kind::none;
        }
        if(node.IsMap()) {
            return node_kind::map;
        }
        if(node.IsSequence()) {
            return node_kind::sequence;
        }
        if(node.IsScalar()) {
            auto tag = node.Tag();
            if(tag == "?") {
                auto sv = node.Scalar();
                if(sv == "true" || sv == "false") {
                    return node_kind::boolean;
                }
                bool has_dot = false;
                bool is_number = !sv.empty();
                for(std::size_t i = 0; i < sv.size(); ++i) {
                    char c = sv[i];
                    if(i == 0 && (c == '-' || c == '+')) {
                        continue;
                    }
                    if(c == '.' || c == 'e' || c == 'E') {
                        has_dot = true;
                        continue;
                    }
                    if(c < '0' || c > '9') {
                        is_number = false;
                        break;
                    }
                }
                if(is_number && !sv.empty()) {
                    return has_dot ? node_kind::floating : node_kind::integer;
                }
            }
            return node_kind::string;
        }
        return node_kind::unknown;
    }

    static codec::type_hint map_to_type_hint(node_kind kind) {
        switch(kind) {
            case node_kind::none: return codec::type_hint::null_like;
            case node_kind::boolean: return codec::type_hint::boolean;
            case node_kind::integer: return codec::type_hint::integer;
            case node_kind::floating: return codec::type_hint::floating;
            case node_kind::string: return codec::type_hint::string;
            case node_kind::sequence: return codec::type_hint::array;
            case node_kind::map: return codec::type_hint::object;
            default: return codec::type_hint::any;
        }
    }

    result_t<YAML::Node> access_node(bool consume) {
        if(!is_valid) {
            return std::unexpected(last_error);
        }
        if(current_value.has_value()) {
            return current_value.get();
        }
        if(root_consumed) {
            return mark_invalid(error_kind::invalid_state);
        }
        if(consume) {
            root_consumed = true;
        }
        return root_node;
    }

    result_t<YAML::Node> peek_node() {
        return access_node(false);
    }

    result_t<YAML::Node> consume_node() {
        return access_node(true);
    }

    std::unexpected<error_type> mark_invalid(error_type err = error_type::invalid_state) {
        is_valid = false;
        if(last_error == error_type::invalid_state || err != error_type::invalid_state) {
            last_error = err;
        }
        return std::unexpected(last_error);
    }

private:
    struct deser_frame {
        node_slot node;
        YAML::const_iterator iter{};
        YAML::const_iterator end_iter{};
        std::string pending_key;
        bool pending_valid = false;
    };

    struct array_frame {
        node_slot node;
        std::size_t index = 0;
    };

    bool is_valid = true;
    bool root_consumed = false;
    error_type last_error = error_type::invalid_state;
    YAML::Node root_node;
    node_slot current_value;
    std::vector<deser_frame> deser_stack;
    std::vector<array_frame> array_stack;
};

template <typename Config = config::default_config, typename T>
auto from_yaml(const YAML::Node& node, T& value) -> std::expected<void, error> {
    Deserializer<Config> deserializer(node);

    KOTA_EXPECTED_TRY(codec::deserialize(deserializer, value));
    KOTA_EXPECTED_TRY(deserializer.finish());
    return {};
}

template <typename T, typename Config = config::default_config>
    requires std::default_initializable<T>
auto from_yaml(const YAML::Node& node) -> std::expected<T, error> {
    T value{};
    KOTA_EXPECTED_TRY(from_yaml<Config>(node, value));
    return value;
}

static_assert(codec::deserializer_like<Deserializer<>>);

}  // namespace kota::codec::yaml
