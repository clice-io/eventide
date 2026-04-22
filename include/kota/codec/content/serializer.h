#pragma once

#include <cmath>
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
#include "kota/codec/content/document.h"
#include "kota/codec/content/error.h"
#include "kota/codec/detail/backend.h"
#include "kota/codec/detail/codec.h"
#include "kota/codec/detail/config.h"

namespace kota::codec::content {

template <typename Config = config::default_config>
class Serializer {
public:
    using config_type = Config;
    using value_type = content::Value;
    using error_type = content::error_kind;

    constexpr static auto backend_kind_v = backend_kind::streaming;
    constexpr static auto field_mode_v = field_mode::by_name;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    Serializer() = default;

    Serializer(const Serializer&) = delete;
    Serializer(Serializer&&) = delete;
    auto operator=(const Serializer&) -> Serializer& = delete;
    auto operator=(Serializer&&) -> Serializer& = delete;

    result_t<value_type> serialize_null() {
        return content::Value(nullptr);
    }

    template <typename T>
    result_t<value_type> serialize_some(const T& value) {
        return codec::serialize(*this, value);
    }

    template <typename... Ts>
    result_t<value_type> serialize_variant(const std::variant<Ts...>& value) {
        return std::visit(
            [&](const auto& item) -> result_t<value_type> { return codec::serialize(*this, item); },
            value);
    }

    result_t<value_type> serialize_bool(bool v) {
        return content::Value(v);
    }

    result_t<value_type> serialize_int(std::int64_t v) {
        return content::Value(v);
    }

    result_t<value_type> serialize_uint(std::uint64_t v) {
        return content::Value(v);
    }

    result_t<value_type> serialize_float(double v) {
        if(std::isfinite(v)) {
            return content::Value(v);
        }
        return content::Value(nullptr);
    }

    result_t<value_type> serialize_char(char v) {
        return content::Value(std::string(1, v));
    }

    result_t<value_type> serialize_str(std::string_view v) {
        return content::Value(v);
    }

    result_t<value_type> serialize_bytes(std::string_view value) {
        KOTA_EXPECTED_TRY(begin_array(value.size()));
        for(unsigned char byte: value) {
            KOTA_EXPECTED_TRY(serialize_element(
                [&] { return serialize_uint(static_cast<std::uint64_t>(byte)); }));
        }
        return end_array();
    }

    result_t<value_type> serialize_bytes(std::span<const std::byte> value) {
        KOTA_EXPECTED_TRY(begin_array(value.size()));
        for(std::byte byte: value) {
            KOTA_EXPECTED_TRY(serialize_element([&] {
                return serialize_uint(
                    static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(byte)));
            }));
        }
        return end_array();
    }

    status_t begin_object(std::size_t /*count*/) {
        stack.push_back(frame{content::Object{}});
        return {};
    }

    result_t<value_type> end_object() {
        if(stack.empty()) {
            return std::unexpected(error_type::invalid_state);
        }
        auto* obj = std::get_if<content::Object>(&stack.back().data);
        if(!obj) {
            return std::unexpected(error_type::invalid_state);
        }
        content::Value result(std::move(*obj));
        stack.pop_back();
        return result;
    }

    status_t field(std::string_view /*name*/) {
        return {};
    }

    status_t begin_array(std::optional<std::size_t> /*count*/) {
        stack.push_back(frame{content::Array{}});
        return {};
    }

    result_t<value_type> end_array() {
        if(stack.empty()) {
            return std::unexpected(error_type::invalid_state);
        }
        auto* arr = std::get_if<content::Array>(&stack.back().data);
        if(!arr) {
            return std::unexpected(error_type::invalid_state);
        }
        content::Value result(std::move(*arr));
        stack.pop_back();
        return result;
    }

    template <typename F>
    status_t serialize_field(std::string_view name, F&& writer) {
        auto result = std::forward<F>(writer)();
        if(!result) {
            return std::unexpected(result.error());
        }
        if(stack.empty()) {
            return std::unexpected(error_type::invalid_state);
        }
        auto* obj = std::get_if<content::Object>(&stack.back().data);
        if(!obj) {
            return std::unexpected(error_type::invalid_state);
        }
        obj->insert(std::string(name), std::move(*result));
        return {};
    }

    template <typename F>
    status_t serialize_element(F&& writer) {
        auto result = std::forward<F>(writer)();
        if(!result) {
            return std::unexpected(result.error());
        }
        if(stack.empty()) {
            return std::unexpected(error_type::invalid_state);
        }
        auto* arr = std::get_if<content::Array>(&stack.back().data);
        if(!arr) {
            return std::unexpected(error_type::invalid_state);
        }
        arr->push_back(std::move(*result));
        return {};
    }

private:
    struct frame {
        std::variant<content::Array, content::Object> data;
    };

    std::vector<frame> stack;
};

static_assert(codec::serializer_like<Serializer<>>);

}  // namespace kota::codec::content

namespace kota::codec {

template <serializer_like S>
struct serialize_traits<S, content::Value> {
    using value_type = typename S::value_type;
    using error_type = typename S::error_type;

    static auto serialize(S& s, const content::Value& value)
        -> std::expected<value_type, error_type> {
        return std::visit(
            [&](const auto& stored) -> std::expected<value_type, error_type> {
                using U = std::remove_cvref_t<decltype(stored)>;
                if constexpr(std::same_as<U, std::monostate>) {
                    return s.serialize_null();
                } else if constexpr(std::same_as<U, bool>) {
                    return s.serialize_bool(stored);
                } else if constexpr(std::same_as<U, std::int64_t>) {
                    return s.serialize_int(stored);
                } else if constexpr(std::same_as<U, std::uint64_t>) {
                    return s.serialize_uint(stored);
                } else if constexpr(std::same_as<U, double>) {
                    return s.serialize_float(stored);
                } else if constexpr(std::same_as<U, std::string>) {
                    return s.serialize_str(std::string_view(stored));
                } else if constexpr(std::same_as<U, content::Array>) {
                    return serialize_traits<S, content::Array>::serialize(s, stored);
                } else {
                    return serialize_traits<S, content::Object>::serialize(s, stored);
                }
            },
            value.variant());
    }
};

template <serializer_like S>
struct serialize_traits<S, content::Array> {
    using value_type = typename S::value_type;
    using error_type = typename S::error_type;

    static auto serialize(S& s, const content::Array& value)
        -> std::expected<value_type, error_type> {
        KOTA_EXPECTED_TRY(s.begin_array(value.size()));
        for(const auto& item: value) {
            KOTA_EXPECTED_TRY(s.serialize_element([&] { return codec::serialize(s, item); }));
        }
        return s.end_array();
    }
};

template <serializer_like S>
struct serialize_traits<S, content::Object> {
    using value_type = typename S::value_type;
    using error_type = typename S::error_type;

    static auto serialize(S& s, const content::Object& value)
        -> std::expected<value_type, error_type> {
        KOTA_EXPECTED_TRY(s.begin_object(value.size()));
        for(const auto& entry: value) {
            KOTA_EXPECTED_TRY(s.serialize_field(std::string_view(entry.key),
                                                [&] { return codec::serialize(s, entry.value); }));
        }
        return s.end_object();
    }
};

template <typename Config>
struct serialize_traits<content::Serializer<Config>, content::Value> {
    using value_type = typename content::Serializer<Config>::value_type;
    using error_type = typename content::Serializer<Config>::error_type;

    static auto serialize(content::Serializer<Config>&, const content::Value& value)
        -> std::expected<value_type, error_type> {
        return value;
    }
};

template <typename Config>
struct serialize_traits<content::Serializer<Config>, content::Array> {
    using value_type = typename content::Serializer<Config>::value_type;
    using error_type = typename content::Serializer<Config>::error_type;

    static auto serialize(content::Serializer<Config>&, const content::Array& value)
        -> std::expected<value_type, error_type> {
        return content::Value(value);
    }
};

template <typename Config>
struct serialize_traits<content::Serializer<Config>, content::Object> {
    using value_type = typename content::Serializer<Config>::value_type;
    using error_type = typename content::Serializer<Config>::error_type;

    static auto serialize(content::Serializer<Config>&, const content::Object& value)
        -> std::expected<value_type, error_type> {
        return content::Value(value);
    }
};

}  // namespace kota::codec
