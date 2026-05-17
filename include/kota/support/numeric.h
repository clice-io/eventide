#pragma once

#include <concepts>
#include <limits>
#include <type_traits>

namespace kota {

/// Narrowing integer cast: returns true and sets `out` if `value` fits in Target.
template <typename Target, typename Source>
    requires std::integral<Target> && (!std::is_const_v<Target>) && std::integral<Source>
constexpr bool narrow_int(Source value, Target& out) {
    static_assert(sizeof(Target) <= sizeof(Source), "not a narrowing conversion");

    bool in_range;
    if constexpr(std::same_as<Target, bool>) {
        in_range = (value == 0) || (value == 1);
    } else if constexpr(std::is_signed_v<Source> == std::is_signed_v<Target>) {
        in_range = value >= (std::numeric_limits<Target>::min)() &&
                   value <= (std::numeric_limits<Target>::max)();
    } else if constexpr(std::is_signed_v<Source>) {
        in_range = value >= 0 && static_cast<std::make_unsigned_t<Source>>(value) <=
                                     static_cast<std::make_unsigned_t<Target>>(
                                         (std::numeric_limits<Target>::max)());
    } else {
        in_range =
            value <=
            static_cast<std::make_unsigned_t<Source>>(
                static_cast<std::make_unsigned_t<Target>>((std::numeric_limits<Target>::max)()));
    }

    if(!in_range) {
        return false;
    }
    out = static_cast<Target>(value);
    return true;
}

}  // namespace kota
