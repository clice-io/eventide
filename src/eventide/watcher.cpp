#include "eventide/watcher.h"

#include <cassert>
#include <chrono>
#include <type_traits>

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"

namespace {

struct timer_tag {};

struct idle_tag {};

struct prepare_tag {};

struct check_tag {};

struct signal_tag {};

template <typename Tag>
struct watcher_traits;

template <>
struct watcher_traits<timer_tag> {
    using watcher_type = eventide::timer;
    using handle_type = uv_timer_t;
};

template <>
struct watcher_traits<idle_tag> {
    using watcher_type = eventide::idle;
    using handle_type = uv_idle_t;
};

template <>
struct watcher_traits<prepare_tag> {
    using watcher_type = eventide::prepare;
    using handle_type = uv_prepare_t;
};

template <>
struct watcher_traits<check_tag> {
    using watcher_type = eventide::check;
    using handle_type = uv_check_t;
};

template <>
struct watcher_traits<signal_tag> {
    using watcher_type = eventide::signal;
    using handle_type = uv_signal_t;
};

}  // namespace

namespace eventide {

template <typename Tag>
struct awaiter {
    using traits = watcher_traits<Tag>;
    using watcher_t = typename traits::watcher_type;
    using handle_t = typename traits::handle_type;
    using promise_t = task<error>::promise_type;

    watcher_t* self;
    error result{};

    static void on_fire(handle_t* handle) {
        auto* watcher = static_cast<watcher_t*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        if(watcher->waiter && watcher->active) {
            *watcher->active = {};

            auto w = watcher->waiter;
            watcher->waiter = nullptr;
            watcher->active = nullptr;

            w->resume();
        } else {
            watcher->pending += 1;
        }
    }

    bool await_ready() const noexcept {
        return self->pending > 0;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        self->active = &result;
        return std::noop_coroutine();
    }

    error await_resume() noexcept {
        if(self->pending > 0) {
            self->pending -= 1;
        }

        self->waiter = nullptr;
        self->active = nullptr;
        return result;
    }
};

template <>
struct awaiter<timer_tag> {
    using traits = watcher_traits<timer_tag>;
    using watcher_t = typename traits::watcher_type;
    using handle_t = typename traits::handle_type;
    using promise_t = task<>::promise_type;

    watcher_t* self;

    static void on_fire(handle_t* handle) {
        auto* watcher = static_cast<watcher_t*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        if(watcher->waiter) {
            auto w = watcher->waiter;
            watcher->waiter = nullptr;
            w->resume();
        } else {
            watcher->pending += 1;
        }
    }

    bool await_ready() const noexcept {
        return self->pending > 0;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        return std::noop_coroutine();
    }

    void await_resume() noexcept {
        if(self->pending > 0) {
            self->pending -= 1;
        }

        self->waiter = nullptr;
    }
};

template <>
struct awaiter<idle_tag> {
    using traits = watcher_traits<idle_tag>;
    using watcher_t = typename traits::watcher_type;
    using handle_t = typename traits::handle_type;
    using promise_t = task<>::promise_type;

    watcher_t* self;

    static void on_fire(handle_t* handle) {
        auto* watcher = static_cast<watcher_t*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        if(watcher->waiter) {
            auto w = watcher->waiter;
            watcher->waiter = nullptr;
            w->resume();
        } else {
            watcher->pending += 1;
        }
    }

    bool await_ready() const noexcept {
        return self->pending > 0;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        return std::noop_coroutine();
    }

    void await_resume() noexcept {
        if(self->pending > 0) {
            self->pending -= 1;
        }

        self->waiter = nullptr;
    }
};

template <>
struct awaiter<prepare_tag> {
    using traits = watcher_traits<prepare_tag>;
    using watcher_t = typename traits::watcher_type;
    using handle_t = typename traits::handle_type;
    using promise_t = task<>::promise_type;

    watcher_t* self;

    static void on_fire(handle_t* handle) {
        auto* watcher = static_cast<watcher_t*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        if(watcher->waiter) {
            auto w = watcher->waiter;
            watcher->waiter = nullptr;
            w->resume();
        } else {
            watcher->pending += 1;
        }
    }

    bool await_ready() const noexcept {
        return self->pending > 0;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        return std::noop_coroutine();
    }

    void await_resume() noexcept {
        if(self->pending > 0) {
            self->pending -= 1;
        }

        self->waiter = nullptr;
    }
};

template <>
struct awaiter<check_tag> {
    using traits = watcher_traits<check_tag>;
    using watcher_t = typename traits::watcher_type;
    using handle_t = typename traits::handle_type;
    using promise_t = task<>::promise_type;

    watcher_t* self;

    static void on_fire(handle_t* handle) {
        auto* watcher = static_cast<watcher_t*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        if(watcher->waiter) {
            auto w = watcher->waiter;
            watcher->waiter = nullptr;
            w->resume();
        } else {
            watcher->pending += 1;
        }
    }

    bool await_ready() const noexcept {
        return self->pending > 0;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        return std::noop_coroutine();
    }

    void await_resume() noexcept {
        if(self->pending > 0) {
            self->pending -= 1;
        }

        self->waiter = nullptr;
    }
};

timer timer::create(event_loop& loop) {
    timer t(sizeof(uv_timer_t));
    auto handle = t.as<uv_timer_t>();
    [[maybe_unused]] int err = uv_timer_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // libuv returns 0 for valid loop/handle; uv_timer_init has no runtime failure path here.
    assert(err == 0 && "uv_timer_init failed: invalid loop/handle");

    t.mark_initialized();
    return t;
}

void timer::start(std::chrono::milliseconds timeout, std::chrono::milliseconds repeat) {
    auto handle = as<uv_timer_t>();
    handle->data = this;
    assert(timeout.count() >= 0 && "timer::start timeout must be non-negative");
    assert(repeat.count() >= 0 && "timer::start repeat must be non-negative");
    [[maybe_unused]] int err = uv_timer_start(
        handle,
        [](uv_timer_t* h) { awaiter<timer_tag>::on_fire(h); },
        static_cast<std::uint64_t>(timeout.count()),
        static_cast<std::uint64_t>(repeat.count()));
    // uv_timer_start only errors if handle is closing or callback is null; we guarantee both.
    assert(err == 0 && "uv_timer_start failed: handle closing or callback null");
}

void timer::stop() {
    auto handle = as<uv_timer_t>();
    [[maybe_unused]] int err = uv_timer_stop(handle);
    // uv_timer_stop is defined to be a no-op for inactive handles and returns 0.
    assert(err == 0 && "uv_timer_stop failed: unexpected libuv error");
}

task<> timer::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return;
    }

    if(waiter != nullptr) {
        assert(false && "timer::wait supports a single waiter at a time");
        co_return;
    }

    co_await awaiter<timer_tag>{this};
}

idle idle::create(event_loop& loop) {
    idle w(sizeof(uv_idle_t));
    auto handle = w.as<uv_idle_t>();
    [[maybe_unused]] int err = uv_idle_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // uv_idle_init does not fail for valid loop/handle.
    assert(err == 0 && "uv_idle_init failed: invalid loop/handle");

    w.mark_initialized();
    return w;
}

void idle::start() {
    auto handle = as<uv_idle_t>();
    handle->data = this;
    [[maybe_unused]] int err =
        uv_idle_start(handle, [](uv_idle_t* h) { awaiter<idle_tag>::on_fire(h); });
    // uv_idle_start only fails for null callback; we always provide one.
    assert(err == 0 && "uv_idle_start failed: callback null");
}

void idle::stop() {
    auto handle = as<uv_idle_t>();
    [[maybe_unused]] int err = uv_idle_stop(handle);
    // uv_idle_stop returns 0 even if the handle is already inactive.
    assert(err == 0 && "uv_idle_stop failed: unexpected libuv error");
}

task<> idle::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return;
    }

    if(waiter != nullptr) {
        assert(false && "idle::wait supports a single waiter at a time");
        co_return;
    }

    co_await awaiter<idle_tag>{this};
}

prepare prepare::create(event_loop& loop) {
    prepare w(sizeof(uv_prepare_t));
    auto handle = w.as<uv_prepare_t>();
    [[maybe_unused]] int err = uv_prepare_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // uv_prepare_init does not fail for valid loop/handle.
    assert(err == 0 && "uv_prepare_init failed: invalid loop/handle");

    w.mark_initialized();
    return w;
}

void prepare::start() {
    auto handle = as<uv_prepare_t>();
    handle->data = this;
    [[maybe_unused]] int err =
        uv_prepare_start(handle, [](uv_prepare_t* h) { awaiter<prepare_tag>::on_fire(h); });
    // uv_prepare_start only fails for null callback; we always provide one.
    assert(err == 0 && "uv_prepare_start failed: callback null");
}

void prepare::stop() {
    auto handle = as<uv_prepare_t>();
    [[maybe_unused]] int err = uv_prepare_stop(handle);
    // uv_prepare_stop returns 0 even if the handle is already inactive.
    assert(err == 0 && "uv_prepare_stop failed: unexpected libuv error");
}

task<> prepare::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return;
    }

    if(waiter != nullptr) {
        assert(false && "prepare::wait supports a single waiter at a time");
        co_return;
    }

    co_await awaiter<prepare_tag>{this};
}

check check::create(event_loop& loop) {
    check w(sizeof(uv_check_t));
    auto handle = w.as<uv_check_t>();
    [[maybe_unused]] int err = uv_check_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // uv_check_init does not fail for valid loop/handle.
    assert(err == 0 && "uv_check_init failed: invalid loop/handle");

    w.mark_initialized();
    return w;
}

void check::start() {
    auto handle = as<uv_check_t>();
    handle->data = this;
    [[maybe_unused]] int err =
        uv_check_start(handle, [](uv_check_t* h) { awaiter<check_tag>::on_fire(h); });
    // uv_check_start only fails for null callback; we always provide one.
    assert(err == 0 && "uv_check_start failed: callback null");
}

void check::stop() {
    auto handle = as<uv_check_t>();
    [[maybe_unused]] int err = uv_check_stop(handle);
    // uv_check_stop returns 0 even if the handle is already inactive.
    assert(err == 0 && "uv_check_stop failed: unexpected libuv error");
}

task<> check::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return;
    }

    if(waiter != nullptr) {
        assert(false && "check::wait supports a single waiter at a time");
        co_return;
    }

    co_await awaiter<check_tag>{this};
}

result<signal> signal::create(event_loop& loop) {
    signal s(sizeof(uv_signal_t));
    auto handle = s.as<uv_signal_t>();
    int err = uv_signal_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    s.mark_initialized();
    return s;
}

error signal::start(int signum) {
    auto handle = as<uv_signal_t>();
    handle->data = this;
    int err = uv_signal_start(
        handle,
        [](uv_signal_t* h, int) { awaiter<signal_tag>::on_fire(h); },
        signum);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error signal::stop() {
    auto handle = as<uv_signal_t>();
    int err = uv_signal_stop(handle);
    if(err != 0) {
        return error(err);
    }

    return {};
}

task<error> signal::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return error{};
    }

    if(waiter != nullptr) {
        co_return error::connection_already_in_progress;
    }

    co_return co_await awaiter<signal_tag>{this};
}

task<> sleep(event_loop& loop, std::chrono::milliseconds timeout) {
    auto t = timer::create(loop);
    t.start(timeout, std::chrono::milliseconds{0});
    co_await t.wait();
}

template struct awaiter<timer_tag>;
template struct awaiter<idle_tag>;
template struct awaiter<prepare_tag>;
template struct awaiter<check_tag>;
template struct awaiter<signal_tag>;

}  // namespace eventide
