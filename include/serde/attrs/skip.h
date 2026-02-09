#pragma once

#include "define.h"

namespace eventide::serde {

// Skip this field during (de)serialization.
template <typename T>
struct skip;

template <warp_type T>
struct skip<T> {
    T value;

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    constexpr static AttrKind kind = AttrKind::skip;
    using value_type = T;
};

template <inherit_type T>
struct skip<T> : T {
    constexpr static AttrKind kind = AttrKind::skip;
    using value_type = T;
};

template <inherit_use_type T>
struct skip<T> : T {
    using T::T;

    constexpr static AttrKind kind = AttrKind::skip;
    using value_type = T;
};

}  // namespace eventide::serde
