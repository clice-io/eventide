#pragma once

#include "context.h"

namespace eventide::serde {

namespace attr {

template <fixed_string... Names>
struct alias {
    constexpr inline static std::array names = {std::string_view(Names)...};
};

}  // namespace attr

template <fixed_string... Names>
struct attr_hook<attr::alias<Names...>> {
    template <typename D, typename V, typename Next>
    constexpr static decltype(auto) process(deserialize_field_probe_ctx<D, V> ctx, Next&& next) {
        for(auto alias_name: attr::alias<Names...>::names) {
            if(alias_name == ctx.key_name) {
                ctx.alias_matched = true;
                break;
            }
        }
        return std::forward<Next>(next)(ctx);
    }
};

}  // namespace eventide::serde
