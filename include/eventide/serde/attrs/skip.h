#pragma once

#include "context.h"

namespace eventide::serde {

namespace attr {

struct skip {};

}  // namespace attr

template <>
struct attr_hook<attr::skip> {
    template <typename S, typename V, typename Next>
    constexpr static auto process(serialize_field_ctx<S, V> ctx, Next&& /*next*/) {
        using result_t = std::invoke_result_t<Next, serialize_field_ctx<S, V>>;
        return result_t{};
    }

    template <typename D, typename V, typename Next>
    constexpr static auto process(deserialize_field_probe_ctx<D, V> ctx, Next&& /*next*/) {
        using result_t = std::invoke_result_t<Next, deserialize_field_probe_ctx<D, V>>;
        return result_t{deserialize_field_probe_decision::no_match};
    }

    template <typename S, typename V, typename Next>
    constexpr static auto process(serialize_value_ctx<S, V> /*ctx*/, Next&& /*next*/) {
        static_assert(dependent_false<V>, "attr::skip is only valid for struct fields");
    }

    template <typename D, typename V, typename Next>
    constexpr static auto process(deserialize_value_ctx<D, V> /*ctx*/, Next&& /*next*/) {
        static_assert(dependent_false<V>, "attr::skip is only valid for struct fields");
    }
};

}  // namespace eventide::serde
