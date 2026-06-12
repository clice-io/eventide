#pragma once

#include <string_view>

#include "kota/codec/visit/context.h"

#if __has_include(<toml++/toml.hpp>)
#include "toml++/toml.hpp"
#else
#error "toml++/toml.hpp not found."
#endif

namespace kota::codec::toml {

using Table = ::toml::table;
using Array = ::toml::array;
using Node = ::toml::node;

using error = rich_error;

namespace detail {

constexpr inline std::string_view boxed_root_key = "__value";

}  // namespace detail

}  // namespace kota::codec::toml
