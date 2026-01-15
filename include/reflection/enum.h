#pragma once

#include <array>
#include <string_view>

#include "name.h"
#include "traits.h"

namespace refl::detail {

template <typename E, std::size_t N = 0>
consteval auto enum_max() {
    constexpr auto value = std::bit_cast<E>(static_cast<std::underlying_type_t<E>>(N));
    if constexpr(enum_name<value>().find(")") == std::string_view::npos) {
        return enum_max<E, N + 1>();
    } else {
        return N;
    }
}

}  // namespace refl::detail

namespace refl {

template <typename T>
struct reflection;

template <traits::enum_type T>
struct reflection<T> {
    constexpr inline static auto member_count = detail::enum_max<T>();

    constexpr inline static auto member_names = []<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::array{enum_name<static_cast<T>(Is)>()...};
    }(std::make_index_sequence<member_count>{});
};

template <traits::enum_type E>
constexpr auto enum_name(E e) {
    return reflection<E>::member_names[static_cast<std::underlying_type_t<E>>(e)];
}

}  // namespace refl
