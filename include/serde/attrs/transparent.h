#pragma once

#include "define.h"

namespace eventide::serde {

// Treat a single-field struct as the underlying field itself.
template <typename T>
struct transparent;

template <warp_type T>
struct transparent<T> {
    T value;

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    constexpr static AttrKind kind = AttrKind::transparent;
    using value_type = T;
};

template <inherit_type T>
struct transparent<T> : T {
    constexpr static AttrKind kind = AttrKind::transparent;
    using value_type = T;
};

template <inherit_use_type T>
struct transparent<T> : T {
    using T::T;

    constexpr static AttrKind kind = AttrKind::transparent;
    using value_type = T;
};

}  // namespace eventide::serde
