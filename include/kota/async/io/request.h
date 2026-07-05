#pragma once

#include <optional>

#include "kota/support/function_traits.h"
#include "kota/support/functional.h"
#include "kota/async/io/loop.h"
#include "kota/async/runtime/task.h"
#include "kota/async/vocab/error.h"

namespace kota {

/// Run work on libuv's worker pool and complete when finished or with an error.
task<void, error> queue(function<void()> fn, event_loop& loop = event_loop::current());

/// Run work on libuv's worker pool, with a hook for chaining cancellation.
///
/// If the awaiting task is cancelled while the work is still queued, the work
/// is dequeued, `fn` never runs, and the hook is not invoked. If `fn` is
/// already running on a pool thread it cannot be interrupted; `on_cancel` is
/// how it learns it should return early — the task settles as cancelled once
/// `fn` returns.
///
/// `on_cancel` runs on the loop thread, at most once, and can still fire
/// after `fn` has already finished. Keep it cheap and idempotent, and only
/// touch state that is safe to share with the concurrently running `fn` —
/// the typical shape is setting an atomic flag that `fn` polls.
task<void, error> queue(function<void()> fn,
                        function<void()> on_cancel,
                        event_loop& loop = event_loop::current());

/// Run work on libuv's worker pool and return either its value or an error.
template <typename Fn, typename R = callable_return_t<Fn>>
    requires std::is_invocable_v<Fn> && (!std::is_void_v<R>)
task<R, error> queue(Fn fn, event_loop& loop = event_loop::current()) {
    std::optional<R> ret;
    co_await queue(function<void()>([&] { ret.emplace(fn()); }), loop).or_fail();
    co_return std::move(*ret);
}

/// Value-returning variant of the cancellation-hook overload.
template <typename Fn, typename R = callable_return_t<Fn>>
    requires std::is_invocable_v<Fn> && (!std::is_void_v<R>)
task<R, error> queue(Fn fn, function<void()> on_cancel, event_loop& loop = event_loop::current()) {
    std::optional<R> ret;
    co_await queue(function<void()>([&] { ret.emplace(fn()); }), std::move(on_cancel), loop)
        .or_fail();
    co_return std::move(*ret);
}

}  // namespace kota
