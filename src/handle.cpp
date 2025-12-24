#include "eventide/handle.h"

#include "libuv.h"

namespace eventide {

handle::handle(std::size_t size) : data(std::malloc(size)) {}

handle::~handle() {
    uv_close(as<uv_handle_t>(), [](uv_handle_t* data) { std::free(data); });
}

bool handle::is_active() {
    return uv_is_active(as<uv_handle_t>());
}

void handle::ref() {
    return uv_ref(as<uv_handle_t>());
}

void handle::unref() {
    return uv_unref(as<uv_handle_t>());
}

}  // namespace eventide
