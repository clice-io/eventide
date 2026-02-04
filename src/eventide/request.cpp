#include "eventide/request.h"

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"
#include "eventide/task.h"

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
        uv_cancel(reinterpret_cast<uv_req_t*>(&self->req));
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

        holder->result = status < 0 ? error(status) : error();
        holder->complete();
    };

    op.result.clear();
    op.req.data = &op;

    int err = uv_queue_work(static_cast<uv_loop_t*>(loop.handle()), &op.req, work_cb, after_cb);
    if(err != 0) {
        co_return error(err);
    }

    co_return co_await op;
}

}  // namespace eventide
