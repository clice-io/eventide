#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "eventide/common/fixed_string.h"
#include "eventide/common/meta.h"
#include "eventide/reflection/struct.h"
#include "eventide/serde/spelling.h"

namespace eventide::serde {

template <typename StructSerializer, typename Value>
struct serialize_field_ctx {
    StructSerializer& s;
    std::string_view name;
    const Value& value;
};

template <typename Serializer, typename Value>
struct serialize_value_ctx {
    Serializer& s;
    const Value& value;
};

enum class deserialize_field_probe_decision {
    no_match,
    match_deferred,
    match_consumed,
};

template <typename StructDeserializer, typename Value>
struct deserialize_field_probe_ctx {
    StructDeserializer& d;
    std::string_view key_name;
    std::string_view field_name;
    Value& value;
    bool alias_matched = false;
};

template <typename StructDeserializer, typename Value>
struct deserialize_field_consume_ctx {
    StructDeserializer& d;
    std::string_view key_name;
    std::string_view field_name;
    Value& value;
};

template <typename Deserializer, typename Value>
struct deserialize_value_ctx {
    Deserializer& d;
    Value& value;
};

template <typename Attr>
struct attr_hook {
    template <typename Ctx, typename Next>
    constexpr static decltype(auto) process(Ctx ctx, Next&& next) {
        return std::forward<Next>(next)(ctx);
    }
};

namespace detail {

template <typename E, typename SerializeStruct, typename Field>
constexpr auto serialize_struct_field(SerializeStruct& s_struct, Field field)
    -> std::expected<void, E>;

template <typename E, typename DeserializeStruct, typename Field>
constexpr auto deserialize_struct_field(DeserializeStruct& d_struct,
                                        std::string_view key_name,
                                        Field field) -> std::expected<bool, E>;

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

template <typename Attr, typename Ctx, typename Next>
constexpr auto try_invoke_attr_hook(int, Ctx ctx, Next&& next)
    -> decltype(attr_hook<Attr>::process(ctx, std::forward<Next>(next))) {
    return attr_hook<Attr>::process(ctx, std::forward<Next>(next));
}

template <typename Attr, typename Ctx, typename Next>
constexpr decltype(auto) try_invoke_attr_hook(long, Ctx ctx, Next&& next) {
    return std::forward<Next>(next)(ctx);
}

template <typename Attr, typename Ctx, typename Next>
constexpr decltype(auto) invoke_attr_hook(Ctx ctx, Next&& next) {
    return try_invoke_attr_hook<Attr>(0, ctx, std::forward<Next>(next));
}

template <typename AttrTuple, std::size_t I = 0, typename Ctx, typename Terminal>
constexpr decltype(auto) run_attr_hook_chain(Ctx ctx, Terminal& terminal) {
    if constexpr(I == std::tuple_size_v<AttrTuple>) {
        return terminal(ctx);
    } else {
        using attr_t = std::tuple_element_t<I, AttrTuple>;
        auto next = [&]<typename NextCtx>(NextCtx next_ctx) -> decltype(auto) {
            return run_attr_hook_chain<AttrTuple, I + 1>(next_ctx, terminal);
        };
        return invoke_attr_hook<attr_t>(ctx, next);
    }
}

}  // namespace detail

}  // namespace eventide::serde
