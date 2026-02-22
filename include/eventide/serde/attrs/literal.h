#pragma once

#include "context.h"

namespace eventide::serde {

namespace attr {

template <fixed_string Name>
struct literal {
    constexpr inline static std::string_view name = Name;
};

}  // namespace attr

}  // namespace eventide::serde
