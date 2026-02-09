#pragma once

#include <array>
#include <string_view>

#include "define.h"

namespace eventide::serde {

// Accept alternative field names during deserialization.
template <typename T, utils::fixed_string... Names>
struct alias;

template <warp_type T, utils::fixed_string... Names>
struct alias<T, Names...> {
    static_assert(sizeof...(Names) > 0, "alias requires at least one name");

    T value;

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    constexpr static AttrKind kind = AttrKind::alias;
    using value_type = T;
    constexpr static std::array<std::string_view, sizeof...(Names)> names = {
        std::string_view(Names)...};
};

template <inherit_type T, utils::fixed_string... Names>
struct alias<T, Names...> : T {
    static_assert(sizeof...(Names) > 0, "alias requires at least one name");

    constexpr static AttrKind kind = AttrKind::alias;
    using value_type = T;
    constexpr static std::array<std::string_view, sizeof...(Names)> names = {
        std::string_view(Names)...};
};

template <inherit_use_type T, utils::fixed_string... Names>
struct alias<T, Names...> : T {
    static_assert(sizeof...(Names) > 0, "alias requires at least one name");

    using T::T;

    constexpr static AttrKind kind = AttrKind::alias;
    using value_type = T;
    constexpr static std::array<std::string_view, sizeof...(Names)> names = {
        std::string_view(Names)...};
};

}  // namespace eventide::serde
