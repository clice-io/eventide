#pragma once

#include <string_view>

#include "define.h"

namespace eventide::serde {

// Constant string literal field (e.g., tagged unions).
template <utils::fixed_string Value>
struct literal {
    constexpr static AttrKind kind = AttrKind::literal;
    constexpr static auto value = Value;
    using value_type = std::string_view;

    constexpr operator std::string_view() const {
        return std::string_view(Value);
    }
};

}  // namespace eventide::serde
