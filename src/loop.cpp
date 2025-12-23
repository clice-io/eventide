#include "eventide/loop.h"

#include "libuv.h"
#include "eventide/task.h"

namespace eventide {

void each(uv_idle_t* idle) {
    auto loop = (event_loop*)idle->data;
    if(loop->idle_running && loop->tasks.empty()) {
        loop->idle_running = false;
        uv_idle_stop(idle);
    }

    /// Resume may create new tasks, we want to run them in the next iteration.
    auto all = std::move(loop->tasks);
    for(auto& task: all) {
        task->resume();
    }
}

void promise_base::schedule() {
    if(!loop->idle_running && loop->tasks.empty()) {
        loop->idle_running = true;
        uv_idle_start((uv_idle_t*)&loop->idle_h, each);
    }

    loop->tasks.push_back(this);
}

event_loop::event_loop() {
    auto loop = (uv_loop_t*)storage;
    int err = uv_loop_init(loop);
    if(err != 0) {
        abort();
    }

    auto idle = (uv_idle_t*)idle_h.storage;
    uv_idle_init(loop, idle);
    uv_idle_start(idle, each);
    idle->data = this;
}

event_loop::~event_loop() {
    constexpr static auto cleanup = +[](uv_handle_t* h, void*) {
        if(!uv_is_closing(h)) {
            uv_close(h, nullptr);
        }
    };

    auto loop = (uv_loop_t*)storage;
    if(uv_loop_close(loop) == UV_EBUSY) {
        uv_walk(loop, cleanup, nullptr);

        /// Run event loop to tiger all cleanup callbacks.
        while(uv_loop_close(loop) == UV_EBUSY) {
            uv_run(loop, UV_RUN_ONCE);
        }
    }

    for(auto task: tasks) {
        if(task->cancelled()) {
            task->resume();
        } else {
            task->destroy();
        }
    }
}

int event_loop::run() {
    auto loop = (uv_loop_t*)storage;
    return uv_run(loop, UV_RUN_DEFAULT);
}

void event_loop::stop() {
    auto loop = (uv_loop_t*)storage;
    uv_stop(loop);
}

}  // namespace eventide
