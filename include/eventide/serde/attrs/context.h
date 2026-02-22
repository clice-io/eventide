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

template <typename AttrTuple, std::size_t I = 0, typename Ctx, typename Terminal>
constexpr decltype(auto) run_attrs_hook(Ctx ctx, Terminal&& terminal) {
    if constexpr(I == std::tuple_size_v<AttrTuple>) {
        return terminal(ctx);
    } else {
        using attr_t = std::tuple_element_t<I, AttrTuple>;
        auto next = [&](auto next_ctx) -> decltype(auto) {
            return run_attrs_hook<AttrTuple, I + 1>(next_ctx, std::forward<Terminal>(terminal));
        };

        if constexpr(requires { attr_hook<attr_t>::process(ctx, next); }) {
            return attr_hook<attr_t>::process(ctx, next);
        } else {
            return next(ctx);
        }
    }
}

}  // namespace eventide::serde
