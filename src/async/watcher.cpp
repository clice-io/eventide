#include "eventide/async/watcher.h"

#include <cassert>
#include <chrono>
#include <memory>

#include "libuv.h"
#include "eventide/async/error.h"
#include "eventide/async/loop.h"

namespace eventide {

struct timer::Self : uv_handle<timer::Self, uv_timer_t> {
    uv_timer_t handle{};
    system_op* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct idle::Self : uv_handle<idle::Self, uv_idle_t> {
    uv_idle_t handle{};
    system_op* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct prepare::Self : uv_handle<prepare::Self, uv_prepare_t> {
    uv_prepare_t handle{};
    system_op* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct check::Self : uv_handle<check::Self, uv_check_t> {
    uv_check_t handle{};
    system_op* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct signal::Self : uv_handle<signal::Self, uv_signal_t> {
    uv_signal_t handle{};
    system_op* waiter = nullptr;
    error* active = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

namespace {

template <typename SelfT, typename HandleT>
struct basic_tick_await : system_op {
    using promise_t = task<>::promise_type;

    SelfT* self;

    explicit basic_tick_await(SelfT* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        detail::cancel_and_complete<basic_tick_await>(op, [](auto& aw) {
            detail::clear_waiter(aw.self, &SelfT::waiter);
        });
    }

    static void on_fire(HandleT* handle) {
        auto* watcher = static_cast<SelfT*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        if(watcher->waiter) {
            auto w = watcher->waiter;
            watcher->waiter = nullptr;
            w->complete();
        } else {
            watcher->pending += 1;
        }
    }

    bool await_ready() const noexcept {
        return self && self->pending > 0;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = this;
        return link_continuation(&waiting.promise(), location);
    }

    void await_resume() noexcept {
        if(self && self->pending > 0) {
            self->pending -= 1;
        }

        if(self) {
            self->waiter = nullptr;
        }
    }
};

using timer_await = basic_tick_await<timer::Self, uv_timer_t>;
using idle_await = basic_tick_await<idle::Self, uv_idle_t>;
using prepare_await = basic_tick_await<prepare::Self, uv_prepare_t>;
using check_await = basic_tick_await<check::Self, uv_check_t>;

struct signal_await : system_op {
    using promise_t = task<error>::promise_type;

    signal::Self* self;
    error result{};

    explicit signal_await(signal::Self* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        detail::cancel_and_complete<signal_await>(op, [](auto& aw) {
            detail::clear_waiter_active(aw.self, &signal::Self::waiter, &signal::Self::active);
        });
    }

    static void on_fire(uv_signal_t* handle) {
        auto* watcher = static_cast<signal::Self*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        if(watcher->waiter && watcher->active) {
            *watcher->active = {};

            auto w = watcher->waiter;
            watcher->waiter = nullptr;
            watcher->active = nullptr;

            w->complete();
        } else {
            watcher->pending += 1;
        }
    }

    bool await_ready() const noexcept {
        return self && self->pending > 0;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = this;
        self->active = &result;
        return link_continuation(&waiting.promise(), location);
    }

    error await_resume() noexcept {
        if(self && self->pending > 0) {
            self->pending -= 1;
        }

        if(self) {
            self->waiter = nullptr;
            self->active = nullptr;
        }
        return result;
    }
};

}  // namespace

timer::timer() noexcept : self(nullptr, nullptr) {}

timer::timer(Self* state) noexcept : self(state, Self::destroy) {}

timer::~timer() = default;

timer::timer(timer&& other) noexcept = default;

timer& timer::operator=(timer&& other) noexcept = default;

timer::Self* timer::operator->() noexcept {
    return self.get();
}

const timer::Self* timer::operator->() const noexcept {
    return self.get();
}

timer timer::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = &state->handle;
    [[maybe_unused]] int err = uv_timer_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // libuv returns 0 for valid loop/handle; uv_timer_init has no runtime failure path here.
    assert(err == 0 && "uv_timer_init failed: invalid loop/handle");

    state->mark_initialized();
    handle->data = state.get();
    return timer(state.release());
}

void timer::start(std::chrono::milliseconds timeout, std::chrono::milliseconds repeat) {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    handle->data = self.get();
    assert(timeout.count() >= 0 && "timer::start timeout must be non-negative");
    assert(repeat.count() >= 0 && "timer::start repeat must be non-negative");
    [[maybe_unused]] int err = uv_timer_start(
        handle,
        [](uv_timer_t* h) { timer_await::on_fire(h); },
        static_cast<std::uint64_t>(timeout.count()),
        static_cast<std::uint64_t>(repeat.count()));
    // uv_timer_start only errors if handle is closing or callback is null; we guarantee both.
    assert(err == 0 && "uv_timer_start failed: handle closing or callback null");
}

void timer::stop() {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    [[maybe_unused]] int err = uv_timer_stop(handle);
    // uv_timer_stop is defined to be a no-op for inactive handles and returns 0.
    assert(err == 0 && "uv_timer_stop failed: unexpected libuv error");
}

task<> timer::wait() {
    if(!self) {
        co_return;
    }

    if(self->pending > 0) {
        self->pending -= 1;
        co_return;
    }

    if(self->waiter != nullptr) {
        assert(false && "timer::wait supports a single waiter at a time");
        co_return;
    }

    co_await timer_await{self.get()};
}

#define EVENTIDE_DEFINE_TICK_WATCHER_METHODS(WatcherType, HandleType, AwaiterType, INIT_FN, START_FN, STOP_FN, NameLiteral) \
    WatcherType WatcherType::create(event_loop& loop) {                                                             \
        std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);                                   \
        auto* handle = &state->handle;                                                                              \
        [[maybe_unused]] int err = INIT_FN(static_cast<uv_loop_t*>(loop.handle()), handle);                        \
        assert(err == 0 && NameLiteral "::create failed: invalid loop/handle");                                    \
                                                                                                                     \
        state->mark_initialized();                                                                                   \
        handle->data = state.get();                                                                                  \
        return WatcherType(state.release());                                                                         \
    }                                                                                                                \
                                                                                                                     \
    void WatcherType::start() {                                                                                      \
        if(!self) {                                                                                                  \
            return;                                                                                                  \
        }                                                                                                            \
                                                                                                                     \
        auto* handle = &self->handle;                                                                                \
        handle->data = self.get();                                                                                   \
        [[maybe_unused]] int err = START_FN(handle, [](HandleType* h) { AwaiterType::on_fire(h); });              \
        assert(err == 0 && NameLiteral "::start failed: callback null");                                            \
    }                                                                                                                \
                                                                                                                     \
    void WatcherType::stop() {                                                                                       \
        if(!self) {                                                                                                  \
            return;                                                                                                  \
        }                                                                                                            \
                                                                                                                     \
        auto* handle = &self->handle;                                                                                \
        [[maybe_unused]] int err = STOP_FN(handle);                                                                  \
        assert(err == 0 && NameLiteral "::stop failed: unexpected libuv error");                                    \
    }                                                                                                                \
                                                                                                                     \
    task<> WatcherType::wait() {                                                                                     \
        if(!self) {                                                                                                  \
            co_return;                                                                                               \
        }                                                                                                            \
                                                                                                                     \
        if(self->pending > 0) {                                                                                      \
            self->pending -= 1;                                                                                      \
            co_return;                                                                                               \
        }                                                                                                            \
                                                                                                                     \
        if(self->waiter != nullptr) {                                                                                \
            assert(false && NameLiteral "::wait supports a single waiter at a time");                              \
            co_return;                                                                                               \
        }                                                                                                            \
                                                                                                                     \
        co_await AwaiterType{self.get()};                                                                            \
    }

idle::idle() noexcept : self(nullptr, nullptr) {}

idle::idle(Self* state) noexcept : self(state, Self::destroy) {}

idle::~idle() = default;

idle::idle(idle&& other) noexcept = default;

idle& idle::operator=(idle&& other) noexcept = default;

idle::Self* idle::operator->() noexcept {
    return self.get();
}

const idle::Self* idle::operator->() const noexcept {
    return self.get();
}

EVENTIDE_DEFINE_TICK_WATCHER_METHODS(
    idle,
    uv_idle_t,
    idle_await,
    uv_idle_init,
    uv_idle_start,
    uv_idle_stop,
    "idle")

prepare::prepare() noexcept : self(nullptr, nullptr) {}

prepare::prepare(Self* state) noexcept : self(state, Self::destroy) {}

prepare::~prepare() = default;

prepare::prepare(prepare&& other) noexcept = default;

prepare& prepare::operator=(prepare&& other) noexcept = default;

prepare::Self* prepare::operator->() noexcept {
    return self.get();
}

const prepare::Self* prepare::operator->() const noexcept {
    return self.get();
}

EVENTIDE_DEFINE_TICK_WATCHER_METHODS(
    prepare,
    uv_prepare_t,
    prepare_await,
    uv_prepare_init,
    uv_prepare_start,
    uv_prepare_stop,
    "prepare")

check::check() noexcept : self(nullptr, nullptr) {}

check::check(Self* state) noexcept : self(state, Self::destroy) {}

check::~check() = default;

check::check(check&& other) noexcept = default;

check& check::operator=(check&& other) noexcept = default;

check::Self* check::operator->() noexcept {
    return self.get();
}

const check::Self* check::operator->() const noexcept {
    return self.get();
}

EVENTIDE_DEFINE_TICK_WATCHER_METHODS(
    check,
    uv_check_t,
    check_await,
    uv_check_init,
    uv_check_start,
    uv_check_stop,
    "check")

#undef EVENTIDE_DEFINE_TICK_WATCHER_METHODS

signal::signal() noexcept : self(nullptr, nullptr) {}

signal::signal(Self* state) noexcept : self(state, Self::destroy) {}

signal::~signal() = default;

signal::signal(signal&& other) noexcept = default;

signal& signal::operator=(signal&& other) noexcept = default;

signal::Self* signal::operator->() noexcept {
    return self.get();
}

const signal::Self* signal::operator->() const noexcept {
    return self.get();
}

result<signal> signal::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = &state->handle;
    int err = uv_signal_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->mark_initialized();
    handle->data = state.get();
    return signal(state.release());
}

error signal::start(int signum) {
    if(!self) {
        return error::invalid_argument;
    }

    auto handle = &self->handle;
    handle->data = self.get();
    int err =
        uv_signal_start(handle, [](uv_signal_t* h, int) { signal_await::on_fire(h); }, signum);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error signal::stop() {
    if(!self) {
        return error::invalid_argument;
    }

    auto handle = &self->handle;
    int err = uv_signal_stop(handle);
    if(err != 0) {
        return error(err);
    }

    return {};
}

task<error> signal::wait() {
    if(!self) {
        co_return error::invalid_argument;
    }

    if(self->pending > 0) {
        self->pending -= 1;
        co_return error{};
    }

    if(self->waiter != nullptr) {
        co_return error::connection_already_in_progress;
    }

    co_return co_await signal_await{self.get()};
}

task<> sleep(std::chrono::milliseconds timeout, event_loop& loop) {
    auto t = timer::create(loop);
    t.start(timeout, std::chrono::milliseconds{0});
    co_await t.wait();
}

}  // namespace eventide
