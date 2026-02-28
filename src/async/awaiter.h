#pragma once

#include <cassert>
#include <deque>
#include <optional>
#include <utility>

#include "eventide/async/error.h"
#include "eventide/async/frame.h"

namespace eventide::detail {

template <typename ResultT>
struct waiter_binding {
    system_op* waiter = nullptr;
    ResultT* active = nullptr;

    bool has_waiter() const noexcept {
        return waiter != nullptr;
    }

    void arm(system_op& op, ResultT& slot) noexcept {
        waiter = &op;
        active = &slot;
    }

    void disarm() noexcept {
        waiter = nullptr;
        active = nullptr;
    }

    bool try_deliver(ResultT&& value) {
        if(waiter == nullptr || active == nullptr) {
            return false;
        }

        *active = std::move(value);
        auto* w = waiter;
        disarm();
        w->complete();
        return true;
    }
};

template <typename SelfT>
inline void clear_waiter(SelfT* self, system_op* SelfT::* waiter) noexcept {
    if(self == nullptr) {
        return;
    }
    self->*waiter = nullptr;
}

template <typename SelfT, typename ActiveT>
inline void clear_waiter_active(SelfT* self,
                                system_op* SelfT::* waiter,
                                ActiveT* SelfT::* active) noexcept {
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
struct queued_delivery : waiter_binding<ResultT> {
    std::deque<ResultT> pending;

    bool has_pending() const noexcept {
        return !pending.empty();
    }

    ResultT take_pending() {
        assert(!pending.empty() && "take_pending requires queued value");
        auto out = std::move(pending.front());
        pending.pop_front();
        return out;
    }

    void deliver(ResultT&& value) {
        if(!this->try_deliver(std::move(value))) {
            pending.push_back(std::move(value));
        }
    }

    void deliver(error err)
        requires requires { ResultT(std::unexpected(err)); }
    {
        deliver(ResultT(std::unexpected(err)));
    }
};

template <typename ResultT>
struct stored_delivery : waiter_binding<ResultT> {
    std::optional<ResultT> pending;

    bool has_pending() const noexcept {
        return pending.has_value();
    }

    ResultT take_pending() {
        assert(pending.has_value() && "take_pending requires stored value");
        auto out = std::move(*pending);
        pending.reset();
        return out;
    }

    void deliver(ResultT&& value) {
        if(!this->try_deliver(std::move(value))) {
            pending = std::move(value);
        }
    }

    void deliver(error err)
        requires requires { ResultT(std::unexpected(err)); }
    {
        deliver(ResultT(std::unexpected(err)));
    }
};

template <typename ResultT>
struct latched_delivery : waiter_binding<ResultT> {
    std::optional<ResultT> pending;

    bool has_pending() const noexcept {
        return pending.has_value();
    }

    const ResultT& peek_pending() const noexcept {
        assert(pending.has_value() && "peek_pending requires latched value");
        return *pending;
    }

    void deliver(ResultT value) {
        pending = value;
        this->try_deliver(std::move(value));
    }

    void deliver(error err)
        requires requires { ResultT(std::unexpected(err)); }
    {
        deliver(ResultT(std::unexpected(err)));
    }
};

template <typename ValueT>
struct latest_value_delivery : waiter_binding<result<ValueT>> {
    std::optional<ValueT> pending;

    bool has_pending() const noexcept {
        return pending.has_value();
    }

    result<ValueT> take_pending() {
        assert(pending.has_value() && "take_pending requires stored success value");
        result<ValueT> out(std::move(*pending));
        pending.reset();
        return out;
    }

    void deliver(result<ValueT>&& value) {
        if(this->waiter != nullptr && this->active != nullptr) {
            this->try_deliver(std::move(value));
            return;
        }

        if(value.has_value()) {
            pending = std::move(*value);
        } else {
            pending.reset();
        }
    }

    void deliver(error err) {
        deliver(std::unexpected(err));
    }
};

}  // namespace eventide::detail
