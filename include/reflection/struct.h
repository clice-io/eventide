#pragma once

#include <array>
#include <string_view>
#include <tuple>

#include "name.h"
#include "traits.h"

namespace refl::detail {

template <class T>
extern const T ext{};

template <class T>
union uninitialized {
    char bytes[sizeof(T)];
    T value;
};

struct any {
    consteval any(std::size_t);

    template <typename T>
    consteval operator T() const;
};

template <typename T, std::size_t N>
consteval auto test() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        return requires { T{any(I)...}; };
    }(std::make_index_sequence<N>{});
}

template <typename T, std::size_t N = 0>
consteval auto field_count() {
    if constexpr(test<T, N>() && !test<T, N + 1>()) {
        return N;
    } else {
        return field_count<T, N + 1>();
    }
}

template <typename T>
union storage_t {
    char dummy;
    T value;

    storage_t() {}

    ~storage_t() {}
};

}  // namespace refl::detail

namespace refl {

template <typename T>
struct reflection;

template <traits::aggregate_type Object>
struct reflection<Object> {
    inline static detail::storage_t<Object> instance;

    constexpr inline static auto field_count = refl::detail::field_count<Object>();

    constexpr static auto field_addrs(auto&& object) {
        // clang-format off
        if constexpr (field_count == 0) {
            return std::tuple{};
        } else if constexpr (field_count == 1) {
            auto&& [e1] = object;
                return std::tuple{ &e1 };
        } else if constexpr (field_count == 2) {
            auto&& [e1, e2] = object;
            return std::tuple{ &e1, &e2 };
        } else if constexpr (field_count == 3) {
            auto&& [e1, e2, e3] = object;
            return std::tuple{ &e1, &e2, &e3 };
        } else if constexpr (field_count == 4) {
            auto&& [e1, e2, e3, e4] = object;
            return std::tuple{ &e1, &e2, &e3, &e4 };
        } else if constexpr (field_count == 5) {
            auto&& [e1, e2, e3, e4, e5] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5 };
        } else if constexpr (field_count == 6) {
            auto&& [e1, e2, e3, e4, e5, e6] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6 };
        } else if constexpr (field_count == 7) {
            auto&& [e1, e2, e3, e4, e5, e6, e7] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7 };
        } else if constexpr (field_count == 8) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8 };
        } else if constexpr (field_count == 9) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9 };
        } else if constexpr (field_count == 10) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10 };
        } else if constexpr (field_count == 11) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11 };
        } else if constexpr (field_count == 12) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12 };
        } else if constexpr (field_count == 13) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13 };
        } else if constexpr (field_count == 14) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14 };
        } else if constexpr (field_count == 15) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15 };
        } else if constexpr (field_count == 16) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16 };
        } else if constexpr (field_count == 17) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17 };
        } else if constexpr (field_count == 18) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18 };
        } else if constexpr (field_count == 19) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19 };
        } else if constexpr (field_count == 20) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20 };
        } else if constexpr (field_count == 21) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21 };
        } else if constexpr (field_count == 22) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22 };
        } else if constexpr (field_count == 23) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23 };
        } else if constexpr (field_count == 24) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24 };
        } else if constexpr (field_count == 25) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25 };
        } else if constexpr (field_count == 26) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26 };
        } else if constexpr (field_count == 27) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27 };
        } else if constexpr (field_count == 28) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28 };
        } else if constexpr (field_count == 29) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29 };
        } else if constexpr (field_count == 30) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29, &e30 };
        } else if constexpr (field_count == 31) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30, e31] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29, &e30, &e31 };
        } else if constexpr (field_count == 32) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30, e31, e32] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29, &e30, &e31, &e32 };
        } else if constexpr (field_count == 33) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30, e31, e32, e33] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29, &e30, &e31, &e32, &e33 };
        } else if constexpr (field_count == 34) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30, e31, e32, e33, e34] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29, &e30, &e31, &e32, &e33, &e34 };
        } else if constexpr (field_count == 35) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30, e31, e32, e33, e34, e35] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29, &e30, &e31, &e32, &e33, &e34, &e35 };
        } else if constexpr (field_count == 36) {
            auto&& [e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30, e31, e32, e33, e34, e35, e36] = object;
            return std::tuple{ &e1, &e2, &e3, &e4, &e5, &e6, &e7, &e8, &e9, &e10, &e11, &e12, &e13, &e14, &e15, &e16, &e17, &e18, &e19, &e20, &e21, &e22, &e23, &e24, &e25, &e26, &e27, &e28, &e29, &e30, &e31, &e32, &e33, &e34, &e35, &e36 };
        } else {
            static_assert(field_count <= 36, "please try to increase the supported member count");
        }
        // clang-format on
    }

    constexpr inline static std::array field_names =
        []<std::size_t... Is>(std::index_sequence<Is...>) {
            if constexpr(field_count == 0) {
                return std::array<std::string_view, 1>{"PLACEHOLDER"};
            } else {
                constexpr auto addrs = field_addrs(instance.value);
                return std::array{refl::field_name<std::get<Is>(addrs)>()...};
            }
        }(std::make_index_sequence<field_count>{});
};

template <typename Object>
constexpr auto field_refs(Object&& object) {
    auto field_addrs = reflection<std::remove_cvref_t<Object>>::field_addrs(object);
    return std::apply(
        [](auto... args) {
            if constexpr(std::is_lvalue_reference_v<Object&&>) {
                return std::tuple<
                    std::add_lvalue_reference_t<std::remove_pointer_t<decltype(args)>>...>(
                    *args...);
            } else {
                return std::tuple<
                    std::add_rvalue_reference_t<std::remove_pointer_t<decltype(args)>>...>(
                    std::move(*args)...);
            }
        },
        field_addrs);
}

template <typename Object>
consteval const auto& field_names() {
    return reflection<Object>::field_names;
}

template <typename Object, std::size_t I>
using field_type =
    std::remove_pointer_t<std::tuple_element_t<I,
                                               decltype(reflection<Object>::field_addrs(
                                                   reflection<Object>::instance.value))>>;

template <std::size_t I, typename Object>
constexpr auto field_addr_of(Object&& object) {
    auto addrs = reflection<std::remove_cvref_t<Object>>::field_addrs(object);
    return std::get<I>(addrs);
}

template <std::size_t I, typename Object>
constexpr auto& field_of(Object&& object) {
    return *field_addr_of<I>(object);
}

template <std::size_t I, typename Object>
consteval std::string_view field_name() {
    return field_names<Object>()[I];
}

template <std::size_t N, class Object>
consteval auto field_offset() noexcept -> std::size_t {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif
    const auto& unknown = detail::ext<detail::uninitialized<Object>>;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    const void* address = field_addr_of<N>(unknown.value);
    for(std::size_t i = 0; i < sizeof(unknown.bytes); ++i) {
        if(address == &unknown.bytes[i]) {
            return i;
        }
    }
#if defined(__clang__)
    __builtin_unreachable();
#endif
}

template <std::size_t I, typename Object>
struct field {
    Object& object;

    constexpr auto&& value() {
        return field_of<I>(object);
    }

    consteval static std::size_t index() {
        return I;
    }

    consteval static std::string_view name() {
        return field_name<I, Object>();
    }

    consteval static std::size_t offset() {
        return field_offset<I, Object>();
    }
};

template <typename Object, typename Callback>
constexpr bool for_each(Object&& object, const Callback& callback) {
    using T = std::remove_cvref_t<Object>;
    using reflect = reflection<std::remove_cvref_t<Object>>;
    auto addrs = reflect::field_addrs(object);
    auto foldable = [&](auto field) {
        using R = decltype(callback(field));
        if constexpr(std::is_void_v<R>) {
            callback(field);
            return true;
        } else {
            return bool(callback(field));
        }
    };

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return (foldable(field<Is, T>{object}) && ...);
    }(std::make_index_sequence<reflect::field_count>());
}

}  // namespace refl
