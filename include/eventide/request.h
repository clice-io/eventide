#pragma once

#include "error.h"
#include "loop.h"
#include "move_only_function.h"

namespace eventide {

task<error> queue(move_only_function<void()> fn, event_loop& loop = event_loop::current());

}  // namespace eventide
