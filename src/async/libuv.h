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

template <typename StatusT>
inline bool is_cancelled_status(StatusT status) noexcept {
    return static_cast<long long>(status) == static_cast<long long>(UV_ECANCELED);
}

template <typename StatusT>
inline bool mark_cancelled_if(system_op* op, StatusT status) noexcept {
    if(op == nullptr || !is_cancelled_status(status)) {
        return false;
    }

    op->state = async_node::Cancelled;
    return true;
}

template <typename StatusT>
inline error status_to_error(StatusT status) noexcept {
    if(static_cast<long long>(status) < 0) {
        return error(static_cast<int>(status));
    }
    return {};
}

template <typename ReqT>
inline void cancel_uv_request(ReqT* req) noexcept {
    if(req == nullptr) {
        return;
    }
    uv_cancel(reinterpret_cast<uv_req_t*>(req));
}

template <typename SelfT>
inline void clear_waiter(SelfT* self, system_op* SelfT::*waiter) noexcept {
    if(self == nullptr) {
        return;
    }
    self->*waiter = nullptr;
}

template <typename SelfT, typename ActiveT>
inline void clear_waiter_active(SelfT* self,
                                system_op* SelfT::*waiter,
                                ActiveT* SelfT::*active) noexcept {
    if(self == nullptr) {
        return;
    }
    self->*waiter = nullptr;
    self->*active = nullptr;
}

template <typename AwaitT, typename CleanupFn>
inline void cancel_and_complete(system_op* op, CleanupFn&& cleanup) noexcept {
    auto* aw = static_cast<AwaitT*>(op);
    if(aw == nullptr) {
        return;
    }

    cleanup(*aw);
    aw->complete();
}

template <typename AwaitT>
inline void cancel_and_complete(system_op* op) noexcept {
    cancel_and_complete<AwaitT>(op, [](AwaitT&) noexcept {});
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
