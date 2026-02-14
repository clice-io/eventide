#pragma once

#include "define.h"

namespace eventide::serde {

// Skip this field during serialization if Pred(value) is true.
template <typename T, typename Pred>
struct skip_if;

template <warp_type T, typename Pred>
struct skip_if<T, Pred> {
    T value;

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    constexpr static AttrKind kind = AttrKind::skip_if;
    using value_type = T;
    using pred_type = Pred;
};

template <inherit_type T, typename Pred>
struct skip_if<T, Pred> : T {
    constexpr static AttrKind kind = AttrKind::skip_if;
    using value_type = T;
    using pred_type = Pred;
};

template <inherit_use_type T, typename Pred>
struct skip_if<T, Pred> : T {
    using T::T;

    constexpr static AttrKind kind = AttrKind::skip_if;
    using value_type = T;
    using pred_type = Pred;
};

}  // namespace eventide::serde
