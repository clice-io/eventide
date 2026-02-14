#pragma once

#include "define.h"

namespace eventide::serde {

// Rename the field key to Name during (de)serialization.
template <typename T, utils::fixed_string Name>
struct rename;

template <warp_type T, utils::fixed_string Name>
struct rename<T, Name> {
    T value;

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    constexpr static AttrKind kind = AttrKind::rename;
    constexpr static auto name = Name;
    using value_type = T;
};

template <inherit_type T, utils::fixed_string Name>
struct rename<T, Name> : T {
    constexpr static AttrKind kind = AttrKind::rename;
    constexpr static auto name = Name;
    using value_type = T;
};

template <inherit_use_type T, utils::fixed_string Name>
struct rename<T, Name> : T {
    using T::T;

    constexpr static AttrKind kind = AttrKind::rename;
    constexpr static auto name = Name;
    using value_type = T;
};

}  // namespace eventide::serde
