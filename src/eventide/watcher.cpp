#include "eventide/watcher.h"

#include <cassert>
#include <chrono>
#include <memory>

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"

namespace eventide {

struct timer::Self : uv_handle<timer::Self, uv_timer_t> {
    uv_timer_t handle{};
    async_node* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct idle::Self : uv_handle<idle::Self, uv_idle_t> {
    uv_idle_t handle{};
    async_node* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct prepare::Self : uv_handle<prepare::Self, uv_prepare_t> {
    uv_prepare_t handle{};
    async_node* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct check::Self : uv_handle<check::Self, uv_check_t> {
    uv_check_t handle{};
    async_node* waiter = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

struct signal::Self : uv_handle<signal::Self, uv_signal_t> {
    uv_signal_t handle{};
    async_node* waiter = nullptr;
    error* active = nullptr;
    int pending = 0;

    Self() {
        handle.data = this;
    }
};

namespace {

struct timer_await : system_op {
    using promise_t = task<>::promise_type;

    timer::Self* self;

    explicit timer_await(timer::Self* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<timer_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
        }
        aw->awaiter = nullptr;
    }

    static void on_fire(uv_timer_t* handle) {
        auto* watcher = static_cast<timer::Self*>(handle->data);
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
        return self && self->pending > 0;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = waiting ? &waiting.promise() : nullptr;
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

struct idle_await : system_op {
    using promise_t = task<>::promise_type;

    idle::Self* self;

    explicit idle_await(idle::Self* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<idle_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
        }
        aw->awaiter = nullptr;
    }

    static void on_fire(uv_idle_t* handle) {
        auto* watcher = static_cast<idle::Self*>(handle->data);
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
        return self && self->pending > 0;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = waiting ? &waiting.promise() : nullptr;
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

struct prepare_await : system_op {
    using promise_t = task<>::promise_type;

    prepare::Self* self;

    explicit prepare_await(prepare::Self* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<prepare_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
        }
        aw->awaiter = nullptr;
    }

    static void on_fire(uv_prepare_t* handle) {
        auto* watcher = static_cast<prepare::Self*>(handle->data);
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
        return self && self->pending > 0;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = waiting ? &waiting.promise() : nullptr;
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

struct check_await : system_op {
    using promise_t = task<>::promise_type;

    check::Self* self;

    explicit check_await(check::Self* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<check_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
        }
        aw->awaiter = nullptr;
    }

    static void on_fire(uv_check_t* handle) {
        auto* watcher = static_cast<check::Self*>(handle->data);
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
        return self && self->pending > 0;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = waiting ? &waiting.promise() : nullptr;
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

struct signal_await : system_op {
    using promise_t = task<error>::promise_type;

    signal::Self* self;
    error result{};

    explicit signal_await(signal::Self* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<signal_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
            aw->self->active = nullptr;
        }
        aw->awaiter = nullptr;
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

            w->resume();
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
        self->waiter = waiting ? &waiting.promise() : nullptr;
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

idle idle::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = &state->handle;
    [[maybe_unused]] int err = uv_idle_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // uv_idle_init does not fail for valid loop/handle.
    assert(err == 0 && "uv_idle_init failed: invalid loop/handle");

    state->mark_initialized();
    handle->data = state.get();
    return idle(state.release());
}

void idle::start() {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    handle->data = self.get();
    [[maybe_unused]] int err = uv_idle_start(handle, [](uv_idle_t* h) { idle_await::on_fire(h); });
    // uv_idle_start only fails for null callback; we always provide one.
    assert(err == 0 && "uv_idle_start failed: callback null");
}

void idle::stop() {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    [[maybe_unused]] int err = uv_idle_stop(handle);
    // uv_idle_stop returns 0 even if the handle is already inactive.
    assert(err == 0 && "uv_idle_stop failed: unexpected libuv error");
}

task<> idle::wait() {
    if(!self) {
        co_return;
    }

    if(self->pending > 0) {
        self->pending -= 1;
        co_return;
    }

    if(self->waiter != nullptr) {
        assert(false && "idle::wait supports a single waiter at a time");
        co_return;
    }

    co_await idle_await{self.get()};
}

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

prepare prepare::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = &state->handle;
    [[maybe_unused]] int err = uv_prepare_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // uv_prepare_init does not fail for valid loop/handle.
    assert(err == 0 && "uv_prepare_init failed: invalid loop/handle");

    state->mark_initialized();
    handle->data = state.get();
    return prepare(state.release());
}

void prepare::start() {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    handle->data = self.get();
    [[maybe_unused]] int err =
        uv_prepare_start(handle, [](uv_prepare_t* h) { prepare_await::on_fire(h); });
    // uv_prepare_start only fails for null callback; we always provide one.
    assert(err == 0 && "uv_prepare_start failed: callback null");
}

void prepare::stop() {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    [[maybe_unused]] int err = uv_prepare_stop(handle);
    // uv_prepare_stop returns 0 even if the handle is already inactive.
    assert(err == 0 && "uv_prepare_stop failed: unexpected libuv error");
}

task<> prepare::wait() {
    if(!self) {
        co_return;
    }

    if(self->pending > 0) {
        self->pending -= 1;
        co_return;
    }

    if(self->waiter != nullptr) {
        assert(false && "prepare::wait supports a single waiter at a time");
        co_return;
    }

    co_await prepare_await{self.get()};
}

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

check check::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = &state->handle;
    [[maybe_unused]] int err = uv_check_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    // uv_check_init does not fail for valid loop/handle.
    assert(err == 0 && "uv_check_init failed: invalid loop/handle");

    state->mark_initialized();
    handle->data = state.get();
    return check(state.release());
}

void check::start() {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    handle->data = self.get();
    [[maybe_unused]] int err =
        uv_check_start(handle, [](uv_check_t* h) { check_await::on_fire(h); });
    // uv_check_start only fails for null callback; we always provide one.
    assert(err == 0 && "uv_check_start failed: callback null");
}

void check::stop() {
    if(!self) {
        return;
    }

    auto handle = &self->handle;
    [[maybe_unused]] int err = uv_check_stop(handle);
    // uv_check_stop returns 0 even if the handle is already inactive.
    assert(err == 0 && "uv_check_stop failed: unexpected libuv error");
}

task<> check::wait() {
    if(!self) {
        co_return;
    }

    if(self->pending > 0) {
        self->pending -= 1;
        co_return;
    }

    if(self->waiter != nullptr) {
        assert(false && "check::wait supports a single waiter at a time");
        co_return;
    }

    co_await check_await{self.get()};
}

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

task<> sleep(event_loop& loop, std::chrono::milliseconds timeout) {
    auto t = timer::create(loop);
    t.start(timeout, std::chrono::milliseconds{0});
    co_await t.wait();
}

}  // namespace eventide
