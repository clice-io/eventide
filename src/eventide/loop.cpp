#include "eventide/loop.h"

#include <cassert>
#include <deque>

#include "libuv.h"
#include "eventide/frame.h"

namespace eventide {

struct event_loop::self {
    uv_loop_t loop = {};
    uv_idle_t idle = {};
    bool idle_running = false;
    std::deque<async_node*> tasks;
};

static thread_local event_loop* current_loop = nullptr;

event_loop* event_loop::current() {
    return current_loop;
}

void each(uv_idle_t* idle) {
    auto self = static_cast<struct event_loop::self*>(idle->data);
    auto loop = &self->loop;
    if(self->idle_running && self->tasks.empty()) {
        self->idle_running = false;
        uv_idle_stop(idle);
    }

    /// Resume may create new tasks, we want to run them in the next iteration.
    auto all = std::move(self->tasks);
    for(auto& task: all) {
        task->resume();
    }
}

void event_loop::schedule(async_node& frame, std::source_location location) {
    assert(self && "schedule: no current event loop in this thread");

    if(frame.state == async_node::Pending) {
        frame.state = async_node::Running;
    } else if(frame.state == async_node::Finished || frame.state == async_node::Running) {
        std::abort();
    } else if(frame.state == async_node::Cancelled) {
        /// meaningless? think cancel shared task.
    }

    frame.location = location;
    auto& self = *this;
    if(!self->idle_running && self->tasks.empty()) {
        self->idle_running = true;
        uv_idle_start(&self->idle, each);
    }
    self->tasks.push_back(&frame);
}

event_loop::event_loop() : self(new struct self()) {
    auto loop = &self->loop;
    int err = uv_loop_init(loop);
    if(err != 0) {
        abort();
    }

    auto idle = &self->idle;
    uv_idle_init(loop, idle);
    uv_idle_start(idle, each);
    idle->data = self.get();
}

event_loop::~event_loop() {
    constexpr static auto cleanup = +[](uv_handle_t* h, void*) {
        if(!uv_is_closing(h)) {
            uv_close(h, nullptr);
        }
    };

    auto loop = &self->loop;
    if(uv_loop_close(loop) == UV_EBUSY) {
        uv_walk(loop, cleanup, nullptr);

        /// Run event loop to tiger all cleanup callbacks.
        while(uv_loop_close(loop) == UV_EBUSY) {
            uv_run(loop, UV_RUN_ONCE);
        }
    }

    for(auto task: self->tasks) {
        /// if(task->is_cancelled()) {
        ///     task->resume();
        /// } else {
        ///     task->destroy();
        /// }
    }
}

void* event_loop::handle() {
    return &self->loop;
}

int event_loop::run() {
    auto previous = current_loop;
    current_loop = this;
    auto result = uv_run(&self->loop, UV_RUN_DEFAULT);
    current_loop = previous;
    return result;
}

void event_loop::stop() {
    uv_stop(&self->loop);
}

}  // namespace eventide
