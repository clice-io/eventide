#pragma once

#include <functional>

#include "eventide/async/io/loop.h"
#include "eventide/async/vocab/error.h"

namespace eventide {

using work_fn = std::function<void()>;

task<error> queue(work_fn fn, event_loop& loop = event_loop::current());

}  // namespace eventide
