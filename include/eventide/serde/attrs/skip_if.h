#pragma once

#include "context.h"

namespace eventide::serde {

namespace detail {

template <typename Pred, typename Value>
constexpr bool evaluate_skip_predicate(const Value& value, bool is_serialize) {
    if constexpr(requires {
                     { Pred{}(value, is_serialize) } -> std::convertible_to<bool>;
                 }) {
        return static_cast<bool>(Pred{}(value, is_serialize));
    } else if constexpr(requires {
                            { Pred{}(value) } -> std::convertible_to<bool>;
                        }) {
        return static_cast<bool>(Pred{}(value));
    } else {
        static_assert(
            dependent_false<Pred>,
            "attr::skip_if predicate must return bool and accept (const Value&, bool) or (const Value&)");
        return false;
    }
}

}  // namespace detail

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
