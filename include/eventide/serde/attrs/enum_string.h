#pragma once

#include "context.h"

namespace eventide::serde {

namespace attr {

template <typename Policy = rename_policy::lower_camel>
struct enum_string {};

}  // namespace attr

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
        static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");

        std::string enum_text;
        auto result = ctx.d.deserialize_value(enum_text);
        if(!result) {
            return std::unexpected(result.error());
        }

        auto parsed = spelling::map_string_to_enum<enum_t, Policy>(enum_text);
        if(parsed.has_value()) {
            ctx.value = *parsed;
        } else {
            ctx.value = enum_t{};
        }
        return {};
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
        static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");

        std::string enum_text;
        auto parsed = ctx.d.deserialize_str(enum_text);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        auto mapped = spelling::map_string_to_enum<enum_t, Policy>(enum_text);
        if(mapped.has_value()) {
            ctx.value = *mapped;
        } else {
            ctx.value = enum_t{};
        }
        return {};
    }
};

}  // namespace eventide::serde
