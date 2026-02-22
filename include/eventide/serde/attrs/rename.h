#pragma once

#include "context.h"

namespace eventide::serde {

namespace attr {

template <fixed_string Name>
struct rename {
    constexpr inline static std::string_view name = Name;
};

}  // namespace attr

template <fixed_string Name>
struct attr_hook<attr::rename<Name>> {
    template <typename S, typename V, typename Next>
    constexpr static decltype(auto) process(serialize_field_ctx<S, V> ctx, Next&& next) {
        ctx.name = Name;
        return std::forward<Next>(next)(ctx);
    }

    template <typename D, typename V, typename Next>
    constexpr static decltype(auto) process(deserialize_field_probe_ctx<D, V> ctx, Next&& next) {
        ctx.field_name = Name;
        return std::forward<Next>(next)(ctx);
    }
};

}  // namespace eventide::serde
