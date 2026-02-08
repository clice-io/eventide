#pragma once

#include <functional>

#include "error.h"
#include "loop.h"

namespace eventide {

task<error> queue(std::move_only_function<void()> fn, event_loop& loop = event_loop::current());

}  // namespace eventide
