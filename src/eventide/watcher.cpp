#include "eventide/watcher.h"

#include <type_traits>

#include "libuv.h"
#include "eventide/async/loop.h"
#include "eventide/error.h"

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
    using promise_t = task<std::error_code>::promise_type;

    watcher_t* self;
    std::error_code result{};

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

    std::error_code await_resume() noexcept {
        if(self->pending > 0) {
            self->pending -= 1;
        }

        self->waiter = nullptr;
        self->active = nullptr;
        return result;
    }
};

std::expected<timer, std::error_code> timer::create(event_loop& loop) {
    timer t(sizeof(uv_timer_t));
    auto handle = t.as<uv_timer_t>();
    int err = uv_timer_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    t.mark_initialized();
    return t;
}

std::error_code timer::start(std::uint64_t timeout_ms, std::uint64_t repeat_ms) {
    auto handle = as<uv_timer_t>();
    handle->data = this;
    int err = uv_timer_start(
        handle,
        [](uv_timer_t* h) { awaiter<timer_tag>::on_fire(h); },
        timeout_ms,
        repeat_ms);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

std::error_code timer::stop() {
    auto handle = as<uv_timer_t>();
    int err = uv_timer_stop(handle);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

task<std::error_code> timer::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return std::error_code{};
    }

    if(waiter != nullptr) {
        co_return uv_error(UV_EALREADY);
    }

    co_return co_await awaiter<timer_tag>{this};
}

std::expected<idle, std::error_code> idle::create(event_loop& loop) {
    idle w(sizeof(uv_idle_t));
    auto handle = w.as<uv_idle_t>();
    int err = uv_idle_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    w.mark_initialized();
    return w;
}

std::error_code idle::start() {
    auto handle = as<uv_idle_t>();
    handle->data = this;
    int err = uv_idle_start(handle, [](uv_idle_t* h) { awaiter<idle_tag>::on_fire(h); });
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

std::error_code idle::stop() {
    auto handle = as<uv_idle_t>();
    int err = uv_idle_stop(handle);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

task<std::error_code> idle::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return std::error_code{};
    }

    if(waiter != nullptr) {
        co_return uv_error(UV_EALREADY);
    }

    co_return co_await awaiter<idle_tag>{this};
}

std::expected<prepare, std::error_code> prepare::create(event_loop& loop) {
    prepare w(sizeof(uv_prepare_t));
    auto handle = w.as<uv_prepare_t>();
    int err = uv_prepare_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    w.mark_initialized();
    return w;
}

std::error_code prepare::start() {
    auto handle = as<uv_prepare_t>();
    handle->data = this;
    int err = uv_prepare_start(handle, [](uv_prepare_t* h) { awaiter<prepare_tag>::on_fire(h); });
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

std::error_code prepare::stop() {
    auto handle = as<uv_prepare_t>();
    int err = uv_prepare_stop(handle);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

task<std::error_code> prepare::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return std::error_code{};
    }

    if(waiter != nullptr) {
        co_return uv_error(UV_EALREADY);
    }

    co_return co_await awaiter<prepare_tag>{this};
}

std::expected<check, std::error_code> check::create(event_loop& loop) {
    check w(sizeof(uv_check_t));
    auto handle = w.as<uv_check_t>();
    int err = uv_check_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    w.mark_initialized();
    return w;
}

std::error_code check::start() {
    auto handle = as<uv_check_t>();
    handle->data = this;
    int err = uv_check_start(handle, [](uv_check_t* h) { awaiter<check_tag>::on_fire(h); });
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

std::error_code check::stop() {
    auto handle = as<uv_check_t>();
    int err = uv_check_stop(handle);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

task<std::error_code> check::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return std::error_code{};
    }

    if(waiter != nullptr) {
        co_return uv_error(UV_EALREADY);
    }

    co_return co_await awaiter<check_tag>{this};
}

std::expected<signal, std::error_code> signal::create(event_loop& loop) {
    signal s(sizeof(uv_signal_t));
    auto handle = s.as<uv_signal_t>();
    int err = uv_signal_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    s.mark_initialized();
    return s;
}

std::error_code signal::start(int signum) {
    auto handle = as<uv_signal_t>();
    handle->data = this;
    int err = uv_signal_start(
        handle,
        [](uv_signal_t* h, int) { awaiter<signal_tag>::on_fire(h); },
        signum);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

std::error_code signal::stop() {
    auto handle = as<uv_signal_t>();
    int err = uv_signal_stop(handle);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

task<std::error_code> signal::wait() {
    if(pending > 0) {
        pending -= 1;
        co_return std::error_code{};
    }

    if(waiter != nullptr) {
        co_return uv_error(UV_EALREADY);
    }

    co_return co_await awaiter<signal_tag>{this};
}

template struct awaiter<timer_tag>;
template struct awaiter<idle_tag>;
template struct awaiter<prepare_tag>;
template struct awaiter<check_tag>;
template struct awaiter<signal_tag>;

}  // namespace eventide
