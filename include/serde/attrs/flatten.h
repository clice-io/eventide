#pragma once

#include "define.h"

namespace eventide::serde {

// Flatten an embedded struct's fields into the parent object.
template <typename T>
struct flatten;

template <inherit_type T>
struct flatten<T> : T {
    constexpr static AttrKind kind = AttrKind::flatten;
    using value_type = T;
};

template <inherit_use_type T>
struct flatten<T> : T {
    using T::T;

    constexpr static AttrKind kind = AttrKind::flatten;
    using value_type = T;
};

}  // namespace eventide::serde
