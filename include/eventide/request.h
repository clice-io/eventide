#pragma once

#include <functional>

#include "error.h"
#include "loop.h"

namespace eventide {

using work_fn = std::function<void()>;

task<error> queue(work_fn fn, event_loop& loop = event_loop::current());

}  // namespace eventide
