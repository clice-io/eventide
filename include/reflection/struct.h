#pragma once

#include <array>
#include <string_view>
#include <tuple>

#include "name.h"

namespace refl::detail {

template <class T>
extern const T ext{};

template <class T>
union uninitialized {
    T value;
    char bytes[sizeof(T)];

    uninitialized() {}

    ~uninitialized() {}
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

}  // namespace refl::detail

namespace refl {

template <typename T>
struct reflection;

template <typename Object>
    requires std::is_aggregate_v<Object>
struct reflection<Object> {
    constexpr inline static auto field_count = refl::detail::field_count<Object>();

    constexpr static auto field_addrs(auto&& object) {
        if constexpr(field_count == 0) {
            return std::tuple{};
        }
#define REFL_BINDING_UNWRAP(...) __VA_ARGS__
#define REFL_BINDING_CASE(COUNT, BINDINGS, ADDRS)                                                  \
    else if constexpr(field_count == COUNT) {                                                      \
        auto&& [REFL_BINDING_UNWRAP BINDINGS] = object;                                            \
        return std::tuple{REFL_BINDING_UNWRAP ADDRS};                                              \
    }
#include "binding.inl"
#undef REFL_BINDING_CASE
#undef REFL_BINDING_UNWRAP
        else {
            static_assert(field_count <= 72, "please try to increase the supported member count");
        }
    }

    constexpr inline static std::array field_names = []<std::size_t... Is>(
                                                         std::index_sequence<Is...>) {
        if constexpr(field_count == 0) {
            return std::array<std::string_view, 1>{"PLACEHOLDER"};
        } else {
            constexpr auto addrs = field_addrs(detail::ext<detail::uninitialized<Object>>.value);
            return std::array{refl::pointer_name<std::get<Is>(addrs)>()...};
        }
    }(std::make_index_sequence<field_count>{});
};

template <typename Object>
consteval std::size_t field_count() {
    return reflection<Object>::field_count;
}

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
using field_type = std::remove_pointer_t<std::tuple_element_t<
    I,
    decltype(reflection<Object>::field_addrs(detail::ext<detail::uninitialized<Object>>.value))>>;

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

    using type = field_type<Object, I>;

    constexpr auto&& value() {
        return field_of<I>(object);
    }

    constexpr static std::size_t index() {
        return I;
    }

    constexpr static std::string_view name() {
        return field_name<I, Object>();
    }

    constexpr static std::size_t offset() {
        return field_offset<I, Object>();
    }
};

template <typename Object, typename Callback>
constexpr bool for_each(Object&& object, const Callback& callback) {
    using reflect = reflection<std::remove_cvref_t<Object>>;
    auto foldable = [&](auto field) {
        using R = decltype(callback(field));
        if constexpr(std::is_void_v<R>) {
            callback(field);
            return true;
        } else {
            return bool(callback(field));
        }
    };

    using T = std::remove_reference_t<Object>;
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return (foldable(field<Is, T>{object}) && ...);
    }(std::make_index_sequence<reflect::field_count>());
}

template <typename T>
concept reflectable_class = requires {
    requires std::is_class_v<T>;
    reflection<T>::field_count;
};

}  // namespace refl
