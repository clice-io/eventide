#pragma once

#include "context.h"

namespace eventide::serde {

namespace attr {

template <typename Policy = rename_policy::lower_camel>
struct enum_string {};

}  // namespace attr

namespace detail {

template <typename Result>
constexpr Result unexpected_invalid_enum() {
    using error_t = typename Result::error_type;
    // TODO(eventide/serde): Introduce a unified serde error taxonomy
    // (e.g. invalid_enum_text / invalid_field_name / invalid_type), so attrs
    // don't need backend-specific or heuristic error-code mapping here.

    if constexpr(requires { error_t::invalid_type; }) {
        return std::unexpected(error_t::invalid_type);
    } else if constexpr(std::is_enum_v<error_t>) {
        // Temporary fallback until standardized serde error codes are available.
        return std::unexpected(static_cast<error_t>(1));
    } else {
        return std::unexpected(error_t{});
    }
}

}  // namespace detail

template <typename Policy>
struct attr_hook<attr::enum_string<Policy>> {
    template <typename S, typename V, typename Next>
    constexpr static auto process(serialize_field_ctx<S, V> ctx, Next&& /*next*/) {
        using enum_t = std::remove_cvref_t<V>;
        static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");

        auto enum_text = spelling::map_enum_to_string<enum_t, Policy>(ctx.value);
        return ctx.s.serialize_field(ctx.name, enum_text);
    }

    template <typename D, typename V, typename Next>
    constexpr static auto process(deserialize_field_consume_ctx<D, V> ctx, Next&& /*next*/)
        -> std::remove_cvref_t<
            decltype(std::declval<D&>().deserialize_value(std::declval<std::string&>()))> {
        using enum_t = std::remove_cvref_t<V>;
        using result_t = std::remove_cvref_t<decltype(std::declval<D&>().deserialize_value(
            std::declval<std::string&>()))>;
        static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");

        std::string enum_text;
        auto result = ctx.d.deserialize_value(enum_text);
        if(!result) {
            return std::unexpected(result.error());
        }

        auto parsed = spelling::map_string_to_enum<enum_t, Policy>(enum_text);
        if(parsed.has_value()) {
            ctx.value = *parsed;
            return result_t{};
        } else {
            return detail::unexpected_invalid_enum<result_t>();
        }
    }

    template <typename S, typename V, typename Next>
    constexpr static auto process(serialize_value_ctx<S, V> ctx, Next&& /*next*/) {
        using enum_t = std::remove_cvref_t<V>;
        static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");

        auto enum_text = spelling::map_enum_to_string<enum_t, Policy>(ctx.value);
        return ctx.s.serialize_str(enum_text);
    }

    template <typename D, typename V, typename Next>
    constexpr static auto process(deserialize_value_ctx<D, V> ctx, Next&& /*next*/)
        -> std::remove_cvref_t<
            decltype(std::declval<D&>().deserialize_str(std::declval<std::string&>()))> {
        using enum_t = std::remove_cvref_t<V>;
        using result_t = std::remove_cvref_t<decltype(std::declval<D&>().deserialize_str(
            std::declval<std::string&>()))>;
        static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");

        std::string enum_text;
        auto parsed = ctx.d.deserialize_str(enum_text);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        auto mapped = spelling::map_string_to_enum<enum_t, Policy>(enum_text);
        if(mapped.has_value()) {
            ctx.value = *mapped;
            return result_t{};
        } else {
            return detail::unexpected_invalid_enum<result_t>();
        }
    }
};

}  // namespace eventide::serde
