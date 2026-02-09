#pragma once

#include "define.h"

namespace eventide::serde {

// Use Codec to customize how this field is (de)serialized.
template <typename T, typename Codec>
struct with;

template <warp_type T, typename Codec>
struct with<T, Codec> {
    T value;

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    constexpr static AttrKind kind = AttrKind::with;
    using value_type = T;
    using codec_type = Codec;
};

template <inherit_type T, typename Codec>
struct with<T, Codec> : T {
    constexpr static AttrKind kind = AttrKind::with;
    using value_type = T;
    using codec_type = Codec;
};

template <inherit_use_type T, typename Codec>
struct with<T, Codec> : T {
    using T::T;

    constexpr static AttrKind kind = AttrKind::with;
    using value_type = T;
    using codec_type = Codec;
};

}  // namespace eventide::serde
