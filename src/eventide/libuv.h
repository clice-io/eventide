#pragma once

#include "uv.h"

namespace eventide {

template <typename Derived, typename Handle>
class uv_handle {
public:
    Handle* handle_ptr() noexcept {
        return &static_cast<Derived*>(this)->handle;
    }

    const Handle* handle_ptr() const noexcept {
        return &static_cast<const Derived*>(this)->handle;
    }

    bool initialized() const noexcept {
        return initialized_;
    }

    void mark_initialized() noexcept {
        initialized_ = true;
    }

    bool is_active() const noexcept {
        if(!initialized_) {
            return false;
        }
        return uv_is_active(reinterpret_cast<const uv_handle_t*>(handle_ptr())) != 0;
    }

    void ref() noexcept {
        if(!initialized_) {
            return;
        }
        uv_ref(reinterpret_cast<uv_handle_t*>(handle_ptr()));
    }

    void unref() noexcept {
        if(!initialized_) {
            return;
        }
        uv_unref(reinterpret_cast<uv_handle_t*>(handle_ptr()));
    }

    static void destroy(void* data) noexcept {
        auto self = static_cast<Derived*>(data);
        if(!self) {
            return;
        }
        if(!self->initialized_) {
            delete self;
            return;
        }

        auto* h = reinterpret_cast<uv_handle_t*>(self->handle_ptr());
        if(uv_is_closing(h)) {
            return;
        }

        uv_close(h, &on_close);
    }

protected:
    uv_handle() = default;
    ~uv_handle() = default;

private:
    static void on_close(uv_handle_t* handle) {
        auto* self = static_cast<Derived*>(handle->data);
        delete self;
    }

    bool initialized_ = false;
};

}  // namespace eventide
