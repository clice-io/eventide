#include "eventide/fs.h"

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"

namespace {

struct fs_event_tag {};

}  // namespace

namespace eventide {

template <>
struct awaiter<fs_event_tag> {
    using promise_t = task<std::expected<fs_event::change, std::error_code>>::promise_type;

    fs_event* self;
    std::expected<fs_event::change, std::error_code> result = std::unexpected(std::error_code{});

    static void on_change(uv_fs_event_t* handle, const char* filename, int events, int status) {
        auto* watcher = static_cast<fs_event*>(handle->data);
        if(watcher == nullptr) {
            return;
        }

        auto deliver = [&](std::expected<fs_event::change, std::error_code>&& value) {
            if(watcher->waiter && watcher->active) {
                *watcher->active = std::move(value);

                auto w = watcher->waiter;
                watcher->waiter = nullptr;
                watcher->active = nullptr;

                w->resume();
            } else {
                if(value.has_value()) {
                    watcher->pending = std::move(value.value());
                } else {
                    watcher->pending.reset();
                }
            }
        };

        if(status < 0) {
            deliver(std::unexpected(uv_error(status)));
            return;
        }

        fs_event::change c{};
        if(filename) {
            c.path = filename;
        }
        c.flags = events;

        deliver(std::move(c));
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        self->active = &result;
        return std::noop_coroutine();
    }

    std::expected<fs_event::change, std::error_code> await_resume() noexcept {
        self->waiter = nullptr;
        self->active = nullptr;
        return std::move(result);
    }
};

std::expected<fs_event, std::error_code> fs_event::create(event_loop& loop) {
    fs_event w(sizeof(uv_fs_event_t));
    auto handle = w.as<uv_fs_event_t>();

    int err = uv_fs_event_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    w.mark_initialized();
    handle->data = &w;
    return w;
}

std::error_code fs_event::start(const char* path, unsigned int flags) {
    auto handle = as<uv_fs_event_t>();
    handle->data = this;
    int err = uv_fs_event_start(handle, awaiter<fs_event_tag>::on_change, path, flags);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

std::error_code fs_event::stop() {
    auto handle = as<uv_fs_event_t>();
    int err = uv_fs_event_stop(handle);
    if(err != 0) {
        return uv_error(err);
    }

    return {};
}

task<std::expected<fs_event::change, std::error_code>> fs_event::wait() {
    if(pending.has_value()) {
        auto out = std::move(*pending);
        pending.reset();
        co_return out;
    }

    if(waiter != nullptr) {
        co_return std::unexpected(uv_error(UV_EALREADY));
    }

    co_return co_await awaiter<fs_event_tag>{this};
}

}  // namespace eventide
