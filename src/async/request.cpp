#include "eventide/async/request.h"

#include "libuv.h"
#include "eventide/async/error.h"
#include "eventide/async/loop.h"
#include "eventide/async/task.h"

namespace eventide {

namespace {

struct work_op : system_op {
    using promise_t = task<error>::promise_type;

    uv_work_t req{};
    work_fn fn;
    error result{};

    work_op() : system_op(async_node::NodeKind::SystemIO) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* self = static_cast<work_op*>(op);
        detail::cancel_uv_request(&self->req);
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        return link_continuation(&waiting.promise(), location);
    }

    error await_resume() noexcept {
        return result;
    }
};

}  // namespace

task<error> queue(work_fn fn, event_loop& loop) {
    work_op op;
    op.fn = std::move(fn);

    auto work_cb = [](uv_work_t* req) {
        auto* holder = static_cast<work_op*>(req->data);
        if(holder && holder->fn) {
            holder->fn();
        }
    };

    auto after_cb = [](uv_work_t* req, int status) {
        auto* holder = static_cast<work_op*>(req->data);
        if(holder == nullptr) {
            return;
        }

        detail::mark_cancelled_if(holder, status);
        holder->result = detail::status_to_error(status);
        holder->complete();
    };

    op.result.clear();
    op.req.data = &op;

    auto err = uv::queue_work(loop.handle(), op.req, work_cb, after_cb);
    if(err) {
        co_return err;
    }

    co_return co_await op;
}

}  // namespace eventide
