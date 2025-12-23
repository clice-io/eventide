#include "eventide/loop.h"

#include "libuv.h"

namespace eventide {

event_loop::event_loop() {
    auto loop = (uv_loop_t*)storage;
    int err = uv_loop_init(loop);
    if(err != 0) {
        abort();
    }
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
