#include "eventide/handle.h"

#include <cstdlib>

#include "libuv.h"

namespace eventide {

handle::handle(std::size_t size) noexcept : data(std::malloc(size)) {}

handle::~handle() noexcept {
    auto handle = as<uv_handle_t>();
    if(!handle) {
        return;
    }

    if(initialized()) {
        // Clean handle->data to avoid accidental reuse after scheduling close.
        handle->data = nullptr;

        // Close and free when libuv invokes the close callback.
        uv_close(handle, [](uv_handle_t* data) { std::free(data); });
    } else {
        std::free(handle);
    }
}

void handle::mark_initialized() noexcept {
    if(!data) [[unlikely]] {
        return;
    }

    auto ptr = reinterpret_cast<std::uintptr_t>(raw_data()) | 1u;
    data = reinterpret_cast<void*>(ptr);
}

bool handle::initialized() const noexcept {
    return (reinterpret_cast<std::uintptr_t>(data) & 1u) != 0;
}

bool handle::is_active() const noexcept {
    if(!initialized()) [[unlikely]] {
        return false;
    }

    return uv_is_active(as<uv_handle_t>());
}

void handle::ref() noexcept {
    if(!initialized()) [[unlikely]] {
        return;
    }

    uv_ref(as<uv_handle_t>());
}

void handle::unref() noexcept {
    if(!initialized()) [[unlikely]] {
        return;
    }

    uv_unref(as<uv_handle_t>());
}

void* handle::raw_data() noexcept {
    auto ptr = reinterpret_cast<std::uintptr_t>(data);
    ptr &= ~std::uintptr_t(1);
    return reinterpret_cast<void*>(ptr);
}

const void* handle::raw_data() const noexcept {
    auto ptr = reinterpret_cast<std::uintptr_t>(data);
    ptr &= ~std::uintptr_t(1);
    return reinterpret_cast<const void*>(ptr);
}

}  // namespace eventide
