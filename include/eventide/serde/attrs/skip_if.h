#pragma once

#include "context.h"

namespace eventide::serde {

namespace attr {

template <typename Pred>
struct skip_if {};

}  // namespace attr

template <typename Pred>
struct attr_hook<attr::skip_if<Pred>> {
    template <typename S, typename V, typename Next>
    constexpr static decltype(auto) process(serialize_field_ctx<S, V> ctx, Next&& next) {
        using result_t = std::invoke_result_t<Next, serialize_field_ctx<S, V>>;
        if(detail::evaluate_skip_predicate<Pred>(ctx.value, true)) {
            return result_t{};
        }
        return std::forward<Next>(next)(ctx);
    }

    template <typename D, typename V, typename Next>
    constexpr static decltype(auto) process(deserialize_field_consume_ctx<D, V> ctx, Next&& next) {
        if(detail::evaluate_skip_predicate<Pred>(ctx.value, false)) {
            return ctx.d.skip_value();
        }
        return std::forward<Next>(next)(ctx);
    }
};

}  // namespace eventide::serde
