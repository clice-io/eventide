#include "eventide/fs.h"

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"

namespace eventide {

struct fs_event::Self : uv_handle<fs_event::Self, uv_fs_event_t> {
    uv_fs_event_t handle{};
    system_op* waiter = nullptr;
    result<fs_event::change>* active = nullptr;
    std::optional<fs_event::change> pending;

    Self() {
        handle.data = this;
    }
};

namespace {

static result<unsigned int> to_uv_fs_event_flags(fs_event::watch_flags flags) {
    unsigned int out = 0;
#ifdef UV_FS_EVENT_WATCH_ENTRY
    if(has_flag(flags, fs_event::watch_flags::watch_entry)) {
        out |= UV_FS_EVENT_WATCH_ENTRY;
    }
#else
    if(has_flag(flags, fs_event::watch_flags::watch_entry)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_FS_EVENT_STAT
    if(has_flag(flags, fs_event::watch_flags::stat)) {
        out |= UV_FS_EVENT_STAT;
    }
#else
    if(has_flag(flags, fs_event::watch_flags::stat)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_FS_EVENT_RECURSIVE
    if(has_flag(flags, fs_event::watch_flags::recursive)) {
        out |= UV_FS_EVENT_RECURSIVE;
    }
#else
    if(has_flag(flags, fs_event::watch_flags::recursive)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

static fs_event::change_flags to_fs_change_flags(int events) {
    auto out = fs_event::change_flags::none;
#ifdef UV_RENAME
    if((events & UV_RENAME) != 0) {
        out |= fs_event::change_flags::rename;
    }
#endif
#ifdef UV_CHANGE
    if((events & UV_CHANGE) != 0) {
        out |= fs_event::change_flags::change;
    }
#endif
    return out;
}

struct fs_event_await : system_op {
    using promise_t = task<result<fs_event::change>>::promise_type;

    fs_event::Self* self;
    result<fs_event::change> outcome = std::unexpected(error{});

    explicit fs_event_await(fs_event::Self* watcher) : self(watcher) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<fs_event_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
            aw->self->active = nullptr;
        }
        aw->complete();
    }

    static void on_change(uv_fs_event_t* handle, const char* filename, int events, int status) {
        auto* watcher = static_cast<fs_event::Self*>(handle->data);
        if(!watcher) {
            return;
        }

        auto deliver = [&](result<fs_event::change>&& value) {
            if(watcher->waiter && watcher->active) {
                *watcher->active = std::move(value);

                auto w = watcher->waiter;
                watcher->waiter = nullptr;
                watcher->active = nullptr;

                w->complete();
            } else {
                if(value.has_value()) {
                    watcher->pending = std::move(value.value());
                } else {
                    watcher->pending.reset();
                }
            }
        };

        if(status < 0) {
            deliver(std::unexpected(error(status)));
            return;
        }

        fs_event::change c{};
        if(filename) {
            c.path = filename;
        }
        c.flags = to_fs_change_flags(events);

        deliver(std::move(c));
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = this;
        self->active = &outcome;
        return link_continuation(&waiting.promise(), location);
    }

    result<fs_event::change> await_resume() noexcept {
        if(self) {
            self->waiter = nullptr;
            self->active = nullptr;
        }
        return std::move(outcome);
    }
};

}  // namespace

fs_event::fs_event() noexcept : self(nullptr, nullptr) {}

fs_event::fs_event(Self* state) noexcept : self(state, Self::destroy) {}

fs_event::~fs_event() = default;

fs_event::fs_event(fs_event&& other) noexcept = default;

fs_event& fs_event::operator=(fs_event&& other) noexcept = default;

fs_event::Self* fs_event::operator->() noexcept {
    return self.get();
}

const fs_event::Self* fs_event::operator->() const noexcept {
    return self.get();
}

result<fs_event> fs_event::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = &state->handle;

    int err = uv_fs_event_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->mark_initialized();
    handle->data = state.get();
    return fs_event(state.release());
}

error fs_event::start(const char* path, watch_flags flags) {
    if(!self) {
        return error::invalid_argument;
    }

    auto uv_flags = to_uv_fs_event_flags(flags);
    if(!uv_flags.has_value()) {
        return uv_flags.error();
    }

    auto handle = &self->handle;
    handle->data = self.get();
    int err = uv_fs_event_start(handle, fs_event_await::on_change, path, uv_flags.value());
    if(err != 0) {
        return error(err);
    }

    return {};
}

error fs_event::stop() {
    if(!self) {
        return error::invalid_argument;
    }

    auto handle = &self->handle;
    int err = uv_fs_event_stop(handle);
    if(err != 0) {
        return error(err);
    }

    return {};
}

task<result<fs_event::change>> fs_event::wait() {
    if(!self) {
        co_return std::unexpected(error::invalid_argument);
    }

    if(self->pending.has_value()) {
        auto out = std::move(*self->pending);
        self->pending.reset();
        co_return out;
    }

    if(self->waiter != nullptr) {
        co_return std::unexpected(error::connection_already_in_progress);
    }

    co_return co_await fs_event_await{self.get()};
}

}  // namespace eventide
