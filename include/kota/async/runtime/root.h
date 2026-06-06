#pragma once

#include <coroutine>
#include <cstdlib>
#include <utility>

#include "kota/support/config.h"
#include "kota/async/runtime/node.h"
#include "kota/async/vocab/awaitable.h"

namespace kota::detail {

struct root_promise;

struct root_task {
    using promise_type = root_promise;
    using handle_type = std::coroutine_handle<root_promise>;

    handle_type h;

    root_task(handle_type h) noexcept : h(h) {}

    root_task(root_task&& o) noexcept : h(std::exchange(o.h, nullptr)) {}

    root_task(const root_task&) = delete;
    root_task& operator=(const root_task&) = delete;

    ~root_task() {
        if(h) {
            if(!defer_frame_destroy(h)) {
                h.destroy();
            }
        }
    }

    void release() noexcept {
        h = nullptr;
    }
};

struct root_promise : root_frame {
    struct final_awaiter {
        bool await_ready() const noexcept {
            return false;
        }

        std::coroutine_handle<>
            await_suspend(std::coroutine_handle<root_promise> h) const noexcept {
            auto& promise = h.promise();
            if(promise.state == async_node::Running) {
                promise.state =
                    promise.propagated_exception ? async_node::Failed : async_node::Finished;
            }
            return promise.finalize();
        }

        [[noreturn]] void await_resume() const noexcept {
            std::abort();
        }
    };

    root_task get_return_object() {
        return {std::coroutine_handle<root_promise>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() noexcept {
        return {};
    }

    final_awaiter final_suspend() noexcept {
        return {};
    }

    void return_void() noexcept {}

#if KOTA_ENABLE_EXCEPTIONS
    void unhandled_exception() noexcept {
        propagated_exception = std::current_exception();
    }
#else
    void unhandled_exception() {
        std::abort();
    }
#endif

    root_promise() {
        this->root = true;
        this->address = std::coroutine_handle<root_promise>::from_promise(*this).address();
    }
};

template <awaitable A>
root_task make_root(A awaitable) {
    co_await std::move(awaitable);
}

struct ref_awaiter {
    task_frame& frame;

    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> h,
                      std::source_location loc = std::source_location::current()) noexcept {
        if(frame.is_cancelled() || frame.is_failed()) {
            return h;
        }
        assert(frame.state == async_node::Pending && "task already scheduled or completed");
        return frame.attach(h.promise(), loc);
    }

    void await_resume() noexcept {}
};

inline root_task make_root_ref(task_frame& frame) {
    co_await ref_awaiter{frame};
}

}  // namespace kota::detail
