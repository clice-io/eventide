#pragma once

#include <string_view>

#include "kota/codec/visit/context.h"

// kotatsu's TOML backend converts every toml++ failure into std::expected /
// rich_error and never lets an exception escape, so toml++ is pinned to its
// no-exceptions mode unconditionally (independent of KOTA_ENABLE_EXCEPTIONS).
// This keeps exactly one parse/error code path across all build flavors and
// keeps EH out of TOML parsing entirely (see PR #168). All translation units
// must agree on this macro before including toml++, or the toml++ ABI
// namespaces (ex/noex) will not match across TUs.
#ifdef TOML_EXCEPTIONS
#if TOML_EXCEPTIONS
#error "kotatsu pins toml++ to TOML_EXCEPTIONS=0; do not predefine TOML_EXCEPTIONS=1"
#endif
#else
#define TOML_EXCEPTIONS 0
#endif

#if __has_include(<toml++/toml.hpp>)
#include "toml++/toml.hpp"
#else
#error "toml++/toml.hpp not found."
#endif

static_assert(!TOML_EXCEPTIONS, "toml++ must be in no-exceptions mode for kotatsu");

namespace kota::codec::toml {

using Table = ::toml::table;
using Array = ::toml::array;
using Node = ::toml::node;

using error = rich_error;

namespace detail {

constexpr inline std::string_view boxed_root_key = "__value";

}  // namespace detail

}  // namespace kota::codec::toml
