#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "eventide/error.h"
#include "serde/core.h"

#if __has_include(<simdjson.h>)
#include <simdjson.h>
#else
#error "simdjson.h not found. Enable EVENTIDE_ENABLE_LANGUAGE or add simdjson include paths."
#endif

namespace eventide::serde::json {}  // namespace eventide::serde::json
