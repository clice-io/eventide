#pragma once

#include <concepts>
#include <type_traits>

#include "../../utils/fixed_string.h"

namespace eventide::serde {

// Wrap or inherit depending on whether T is an aggregate class.
template <typename T>
concept warp_type = !std::is_class_v<T> || std::is_final_v<T>;

// Aggregate class that can be inherited without losing aggregate-ness.
template <typename T>
concept inherit_type = std::is_aggregate_v<T> && !warp_type<T>;

// Non-aggregate class where inheriting and reusing constructors is desired.
template <typename T>
concept inherit_use_type = !std::is_aggregate_v<T> && !warp_type<T>;

// Attribute categories used by compile-time dispatch.
enum class AttrKind {
    skip,
    skip_if,
    rename,
    flatten,
    alias,
    default_value,
    literal,
    with,
    transparent,
};

}  // namespace eventide::serde
