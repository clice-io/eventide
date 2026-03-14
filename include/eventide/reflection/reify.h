#pragma once

#include <span>
#include <string_view>
#include <type_traits>

#include "enum.h"
#include "struct.h"

namespace eventide::refl {

struct type_info;

using type_id = const type_info*;

struct field_info {
    type_id type;
    std::string_view name;
    std::size_t offset;
};

struct type_info {
    std::size_t size;
    std::size_t alignment;
    std::span<const field_info> fields;
};

template <type_id value>
struct reader {
    friend consteval auto reification(reader);
};

template <type_id value, typename T>
struct setter {
    friend consteval auto reification(reader<value>) {
        return std::type_identity<T>();
    }
};

template <type_id value>
struct cache {
    using type = typename decltype(reification(reader<value>()))::type;
};

template <typename T>
struct metadata {
    constexpr inline static type_info value = {
        sizeof(T),
        alignof(T),
    };
};

template <typename T>
consteval type_id reify() {
    constexpr auto id = type_id(&metadata<T>::value);
    setter<id, T> _ [[maybe_unused]];
    return id;
}

template <refl::reflectable_class T>
struct metadata<T> {
    constexpr inline static auto fields = [] {
        std::array<field_info, field_count<T>()> out;
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((out[I] =
                  field_info{
                      .type = reify<field_type<T, I>>(),
                      .name = field_name<I, T>(),
                      .offset = field_offset<T>(I),
                  }),
             ...);
        }(std::make_index_sequence<field_count<T>()>{});
        return out;
    }();

    constexpr inline static type_info value = {
        sizeof(T),
        alignof(T),
        fields,
    };
};

template <type_id value>
using unreify = typename cache<value>::type;

}  // namespace eventide::refl
