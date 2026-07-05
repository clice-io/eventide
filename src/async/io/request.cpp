#include "kota/async/io/request.h"

#include <cassert>

#include "awaiter.h"
#include "kota/async/io/loop.h"
#include "kota/async/runtime/task.h"
#include "kota/async/vocab/error.h"

namespace kota {

namespace {

struct work_op : uv::await_op<work_op> {
    using promise_t = task<void, error>::promise_type;

    // libuv request object; req.data points back to this awaiter.
    uv_work_t req{};
    // User-supplied function executed on libuv's worker thread.
    function<void()> fn;
    // Invoked on the loop thread when the awaiting task is cancelled, so an
    // already-running fn can observe cancellation and return early. Always
    // set; the hook-less overload passes a no-op.
    function<void()> cancel_hook;
    // Completion status consumed by await_resume().
    error result;

    work_op(function<void()> fn, function<void()> cancel_hook) :
        fn(std::move(fn)), cancel_hook(std::move(cancel_hook)) {}

    static void on_cancel(io_op* op) {
        auto* self = static_cast<work_op*>(op);
        // Dequeue first so the hook cannot indirectly free a pool thread that
        // would pick this work up before uv_cancel runs. If dequeuing fails
        // the work is running (or just finished); the hook tells it to return
        // early.
        if(uv::cancel(self->req)) {
            self->cancel_hook();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location loc = std::source_location::current()) noexcept {
        return this->attach(waiting.promise(), loc);
    }

    error await_resume() noexcept {
        return result;
    }
};

}  // namespace

task<void, error> queue(function<void()> fn, function<void()> on_cancel, event_loop& loop) {
    work_op op(std::move(fn), std::move(on_cancel));

    auto work_cb = [](uv_work_t* req) {
        auto* holder = static_cast<work_op*>(req->data);
        assert(holder != nullptr && "work_cb requires operation in req->data");
        holder->fn();
    };

    auto after_cb = [](uv_work_t* req, int status) {
        auto* holder = static_cast<work_op*>(req->data);
        assert(holder != nullptr && "after_cb requires operation in req->data");

        holder->mark_cancelled_if(status);
        holder->result = uv::status_to_error(status);
        holder->complete();
    };

    op.result.clear();
    op.req.data = &op;

    if(auto err = uv::queue_work(loop, op.req, work_cb, after_cb)) {
        co_await fail(err);
    }

    if(auto err = co_await op) {
        co_await fail(std::move(err));
    }
}

task<void, error> queue(function<void()> fn, event_loop& loop) {
    return queue(std::move(fn), function<void()>([] {}), loop);
}

}  // namespace kota
