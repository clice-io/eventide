#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "backend.h"
#include "config.h"
#include "kota/support/expected_try.h"
#include "kota/support/ranges.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/meta/enum.h"
#include "kota/meta/struct.h"
#include "kota/codec/detail/common.h"
#include "kota/codec/detail/deser_dispatch.h"
#include "kota/codec/detail/dispatch.h"
#include "kota/codec/detail/fwd.h"
#include "kota/codec/detail/struct_serialize.h"

namespace kota::codec {

template <serializer_like S, typename V, typename T, typename E>
constexpr auto serialize(S& s, const V& v) -> std::expected<T, E> {
    using Serde = serialize_traits<S, V>;

    if constexpr(requires { Serde::serialize(s, v); }) {
        return Serde::serialize(s, v);
    } else {
        detail::StreamingCtx<S> ctx{s};
        return detail::
            unified_serialize<config::config_of<S>, detail::StreamingCtx<S>, std::tuple<>>(ctx, v);
    }
}

template <deserializer_like D, typename V, typename E>
constexpr auto deserialize(D& d, V& v) -> std::expected<void, E> {
    using Deserde = deserialize_traits<D, V>;

    if constexpr(requires { Deserde::deserialize(d, v); }) {
        return Deserde::deserialize(d, v);
    } else {
        detail::StreamingDeserCtx<D> ctx{d};
        return detail::unified_deserialize<config::config_of<D>,
                                           detail::StreamingDeserCtx<D>,
                                           std::tuple<>>(ctx, v);
    }
}

}  // namespace kota::codec
