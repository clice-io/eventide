#pragma once

#include "backend.h"
#include "config.h"
#include "kota/codec/detail/deser_dispatch.h"

namespace kota::codec {

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
