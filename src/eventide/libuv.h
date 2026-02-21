#pragma once

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "uv.h"
#include "eventide/async/error.h"
#include "eventide/async/frame.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

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

namespace detail {

struct resolved_addr {
    sockaddr_storage storage{};
    int family = AF_UNSPEC;
};

inline result<resolved_addr> resolve_addr(std::string_view host, int port) {
    std::string host_storage(host);
    resolved_addr out{};

    if(uv_ip6_addr(host_storage.c_str(), port, reinterpret_cast<sockaddr_in6*>(&out.storage)) ==
       0) {
        out.family = AF_INET6;
        return out;
    }

    if(uv_ip4_addr(host_storage.c_str(), port, reinterpret_cast<sockaddr_in*>(&out.storage)) == 0) {
        out.family = AF_INET;
        return out;
    }

    return std::unexpected(error::invalid_argument);
}

template <typename ResultT>
inline void deliver_or_queue(system_op*& waiter,
                             ResultT*& active,
                             std::deque<ResultT>& pending,
                             ResultT&& value) {
    if(waiter && active) {
        *active = std::move(value);
        auto w = waiter;
        waiter = nullptr;
        active = nullptr;
        w->complete();
        return;
    }

    pending.push_back(std::move(value));
}

template <typename ResultT>
inline void deliver_or_store(system_op*& waiter,
                             ResultT*& active,
                             std::optional<ResultT>& pending,
                             ResultT&& value) {
    if(waiter && active) {
        *active = std::move(value);
        auto w = waiter;
        waiter = nullptr;
        active = nullptr;
        w->complete();
        return;
    }

    pending = std::move(value);
}

}  // namespace detail

}  // namespace eventide
