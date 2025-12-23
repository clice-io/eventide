#include "eventide/handle.h"

#include "libuv.h"
#include "eventide/stream.h"

namespace eventide {

template <typename Derived>
bool handle<Derived>::is_active() const {
    auto h = (const uv_handle_t*)this->native_handle();
    return uv_is_active(h);
}

template <typename Derived>
void handle<Derived>::close() {
    auto h = (uv_handle_t*)this->native_handle();
    uv_close(h, nullptr);
}

template <typename Derived>
void handle<Derived>::ref() {
    auto h = (uv_handle_t*)this->native_handle();
    uv_ref(h);
}

template <typename Derived>
void handle<Derived>::unref() {
    auto h = (uv_handle_t*)this->native_handle();
    uv_is_active(h);
}

template class handle<pipe>;
template class handle<tcp_socket>;
template class handle<console>;

}  // namespace eventide
