#pragma once

#include "context.h"

namespace eventide::serde {

namespace detail {

template <typename E, typename SerializeStruct, typename Field>
constexpr auto serialize_struct_field(SerializeStruct& s_struct, Field field)
    -> std::expected<void, E>;

template <typename E, typename DeserializeStruct, typename Field>
constexpr auto deserialize_struct_field(DeserializeStruct& d_struct,
                                        std::string_view key_name,
                                        Field field) -> std::expected<bool, E>;

}  // namespace detail

namespace attr {

struct flatten {};

}  // namespace attr

template <>
struct attr_hook<attr::flatten> {
    template <typename S, typename V, typename Next>
    constexpr static auto process(serialize_field_ctx<S, V> ctx, Next&& /*next*/) {
        using nested_t = std::remove_cvref_t<V>;
        static_assert(refl::reflectable_class<nested_t>,
                      "attr::flatten requires a reflectable class field type");
        using error_t =
            typename std::remove_cvref_t<decltype(ctx.s.serialize_field(ctx.name,
                                                                        ctx.value))>::error_type;

        std::expected<void, error_t> nested_result;
        refl::for_each(ctx.value, [&](auto nested_field) {
            auto status = detail::serialize_struct_field<error_t>(ctx.s, nested_field);
            if(!status) {
                nested_result = std::unexpected(status.error());
                return false;
            }
            return true;
        });
        return nested_result;
    }

    template <typename D, typename V, typename Next>
    constexpr static auto process(deserialize_field_probe_ctx<D, V> ctx, Next&& /*next*/)
        -> std::expected<
            deserialize_field_probe_decision,
            typename std::remove_cvref_t<decltype(std::declval<D&>().skip_value())>::error_type> {
        using nested_t = std::remove_cvref_t<V>;
        static_assert(refl::reflectable_class<nested_t>,
                      "attr::flatten requires a reflectable class field type");
        using error_t = typename std::remove_cvref_t<decltype(ctx.d.skip_value())>::error_type;

        std::expected<void, error_t> nested_error;
        bool matched = false;
        refl::for_each(ctx.value, [&](auto nested_field) {
            auto status =
                detail::deserialize_struct_field<error_t>(ctx.d, ctx.key_name, nested_field);
            if(!status) {
                nested_error = std::unexpected(status.error());
                return false;
            }
            if(*status) {
                matched = true;
                return false;
            }
            return true;
        });
        if(!nested_error) {
            return std::unexpected(nested_error.error());
        }

        if(matched) {
            return deserialize_field_probe_decision::match_consumed;
        }
        return deserialize_field_probe_decision::no_match;
    }

    template <typename S, typename V, typename Next>
    constexpr static auto process(serialize_value_ctx<S, V> /*ctx*/, Next&& /*next*/) {
        static_assert(dependent_false<V>, "attr::flatten is only valid for struct fields");
    }

    template <typename D, typename V, typename Next>
    constexpr static auto process(deserialize_value_ctx<D, V> /*ctx*/, Next&& /*next*/) {
        static_assert(dependent_false<V>, "attr::flatten is only valid for struct fields");
    }
};

}  // namespace eventide::serde
