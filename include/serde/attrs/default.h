#pragma once

#include "define.h"

namespace eventide::serde {

// Provide a default value when a field is missing.
template <typename T>
struct default_value;

template <warp_type T>
struct default_value<T> {
    T value;

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    constexpr static AttrKind kind = AttrKind::default_value;
    using value_type = T;
};

template <inherit_type T>
struct default_value<T> : T {
    constexpr static AttrKind kind = AttrKind::default_value;
    using value_type = T;
};

template <inherit_use_type T>
struct default_value<T> : T {
    using T::T;

    constexpr static AttrKind kind = AttrKind::default_value;
    using value_type = T;
};

template <typename T>
using default_ = default_value<T>;

}  // namespace eventide::serde
