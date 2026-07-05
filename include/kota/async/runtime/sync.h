#pragma once

#include <cassert>
#include <cstddef>
#include <source_location>

#include "kota/async/runtime/node.h"
#include "kota/async/runtime/task.h"

namespace kota {

/// Shared base for synchronization resources. These are not awaitable runtime
/// nodes; wait_node sub-objects bridge tasks into the wait queue.
class sync_primitive {
public:
    enum class Kind : std::uint8_t {
        Mutex,
        Event,
        Semaphore,
        ConditionVariable,
    };

    friend class async_node;

    explicit sync_primitive(Kind k) : kind(k) {}

    const Kind kind;

    std::source_location location;

    /// Appends a waiter to the end of the wait queue.
    void insert(wait_node* link);

    /// Removes a waiter from the wait queue.
    void remove(wait_node* link);

    const wait_node* get_head() const noexcept {
        return head;
    }

protected:
    bool has_waiters() const noexcept {
        return head != nullptr;
    }

    wait_node* pop_waiter() noexcept {
        auto* link = head;
        if(link) {
            remove(link);
        }
        return link;
    }

    bool resume_waiter(wait_node& link) noexcept;

    bool cancel_waiter(wait_node& link) noexcept;

    /// Pops and processes every queued waiter.
    ///
    /// resume_waiter/cancel_waiter never run user code — they only tag the
    /// waiter and queue its awaiting task on the event loop — so no waiter
    /// can be enqueued or removed re-entrantly while this loop runs: popping
    /// until the queue is empty is a stable snapshot of the callers.
    template <typename Fn>
    void drain_waiters(Fn&& fn) {
        while(auto* waiter = pop_waiter()) {
            fn(*waiter);
        }
    }

private:
    /// Head and tail of the intrusive doubly-linked waiter queue.
    wait_node* head = nullptr;
    wait_node* tail = nullptr;
};

class mutex : public sync_primitive {
public:
    mutex(std::source_location location = std::source_location::current()) :
        sync_primitive(sync_primitive::Kind::Mutex) {
        this->location = location;
    }

    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

    struct lock_awaiter : wait_node {
        explicit lock_awaiter(mutex& owner) :
            wait_node(async_node::NodeKind::MutexWaiter), owner(&owner) {
            abandon_context = &owner;
            abandon = &abandon_grant;
        }

        bool await_ready() noexcept {
            return owner->try_lock();
        }

        template <typename Promise>
        auto await_suspend(
            std::coroutine_handle<Promise> h,
            std::source_location location = std::source_location::current()) noexcept {
            owner->insert(this);
            return attach(h.promise(), location);
        }

        void await_resume() noexcept {
            abandon_context = nullptr;
            abandon = nullptr;
        }

    private:
        static void abandon_grant(void* context) noexcept {
            if(auto* owner = static_cast<mutex*>(context)) {
                owner->unlock();
            }
        }

        mutex* owner = nullptr;
    };

    lock_awaiter lock() noexcept {
        return lock_awaiter(*this);
    }

    bool try_lock() noexcept {
        if(locked) {
            return false;
        }
        locked = true;
        return true;
    }

    void unlock() noexcept {
        assert(locked && "mutex::unlock without lock");
        while(auto* waiter = pop_waiter()) {
            if(resume_waiter(*waiter)) {
                return;
            }
        }
        locked = false;
    }

private:
    bool locked = false;
};

class semaphore : public sync_primitive {
public:
    explicit semaphore(std::ptrdiff_t initial = 0,
                       std::source_location location = std::source_location::current()) :
        sync_primitive(sync_primitive::Kind::Semaphore) {
        assert(initial >= 0 && "semaphore initial count must be non-negative");
        this->location = location;
        count = initial;
    }

    semaphore(const semaphore&) = delete;
    semaphore& operator=(const semaphore&) = delete;

    struct acquire_awaiter : wait_node {
        /// Reuses EventWaiter kind — all wait_node subtypes share identical
        /// cancel/attach/finalize logic, so a dedicated
        /// SemaphoreWaiter kind is unnecessary.
        explicit acquire_awaiter(semaphore& owner) :
            wait_node(async_node::NodeKind::EventWaiter), owner(&owner) {
            abandon_context = &owner;
            abandon = &abandon_grant;
        }

        bool await_ready() noexcept {
            return owner->try_acquire();
        }

        template <typename Promise>
        auto await_suspend(
            std::coroutine_handle<Promise> h,
            std::source_location location = std::source_location::current()) noexcept {
            owner->insert(this);
            return attach(h.promise(), location);
        }

        void await_resume() noexcept {
            abandon_context = nullptr;
            abandon = nullptr;
        }

    private:
        static void abandon_grant(void* context) noexcept {
            if(auto* owner = static_cast<semaphore*>(context)) {
                owner->release();
            }
        }

        semaphore* owner = nullptr;
    };

    acquire_awaiter acquire() noexcept {
        return acquire_awaiter(*this);
    }

    bool try_acquire() noexcept {
        if(count <= 0) {
            return false;
        }
        count -= 1;
        return true;
    }

    void release(std::ptrdiff_t n = 1) {
        assert(n >= 0 && "semaphore::release count must be non-negative");
        for(std::ptrdiff_t i = 0; i < n; ++i) {
            bool transferred = false;
            if(has_waiters()) {
                while(auto* waiter = pop_waiter()) {
                    if(resume_waiter(*waiter)) {
                        transferred = true;
                        break;
                    }
                }
            }

            if(!transferred) {
                count += 1;
            }
        }
    }

private:
    std::ptrdiff_t count = 0;
};

class event : public sync_primitive {
public:
    explicit event(bool signaled = false,
                   std::source_location location = std::source_location::current()) :
        sync_primitive(sync_primitive::Kind::Event), signaled(signaled) {
        this->location = location;
    }

    event(const event&) = delete;
    event& operator=(const event&) = delete;

    struct wait_awaiter : wait_node {
        explicit wait_awaiter(event& owner) :
            wait_node(async_node::NodeKind::EventWaiter), owner(&owner) {}

        bool await_ready() noexcept {
            return owner->is_set();
        }

        template <typename Promise>
        auto await_suspend(
            std::coroutine_handle<Promise> h,
            std::source_location location = std::source_location::current()) noexcept {
            owner->insert(this);
            return attach(h.promise(), location);
        }

        outcome<void, void, cancellation> await_resume() noexcept {
            if(this->state == async_node::Cancelled) {
                return outcome_cancel(cancellation{});
            }

            return {};
        }

    private:
        event* owner = nullptr;
    };

    /// Waits until the event is set. If the current wait queue is interrupted,
    /// cancellation is propagated through the returned task.
    task<> wait() {
        auto result = co_await wait_awaiter(*this);
        if(result.is_cancelled()) {
            co_await cancel();
        }
    }

    void set() noexcept {
        signaled = true;
        drain_waiters([this](wait_node& waiter) { resume_waiter(waiter); });
    }

    void reset() noexcept {
        signaled = false;
    }

    /// Interrupts the current wait queue without changing the signaled state.
    void interrupt() noexcept {
        drain_waiters([this](wait_node& waiter) { cancel_waiter(waiter); });
    }

    bool is_set() const noexcept {
        return signaled;
    }

private:
    bool signaled = false;
};

class condition_variable : public sync_primitive {
public:
    condition_variable(std::source_location location = std::source_location::current()) :
        sync_primitive(sync_primitive::Kind::ConditionVariable) {
        this->location = location;
    }

    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    struct wait_awaiter : wait_node {
        /// Reuses EventWaiter kind — see semaphore::acquire_awaiter comment.
        explicit wait_awaiter(condition_variable& owner) :
            wait_node(async_node::NodeKind::EventWaiter), owner(&owner) {}

        bool await_ready() const noexcept {
            return false;
        }

        template <typename Promise>
        auto await_suspend(
            std::coroutine_handle<Promise> h,
            std::source_location location = std::source_location::current()) noexcept {
            owner->insert(this);
            return attach(h.promise(), location);
        }

        void await_resume() noexcept {}

    private:
        condition_variable* owner = nullptr;
    };

    /// Atomically unlocks `m`, waits for a notification, then re-locks `m`.
    ///
    /// Cancellation note: if this task is cancelled while suspended on the
    /// wait_awaiter, the mutex will NOT be re-acquired (co_await m.lock() is
    /// never reached). In normal usage this is safe because cancellation
    /// propagates upward — the caller is also cancelled and never observes
    /// the unlocked mutex. However, if cancellation is intercepted externally
    /// (e.g., catch_cancel() or with_token() wrapping a cv.wait() call), the
    /// caller resumes with the mutex NOT held. Avoid intercepting cancellation
    /// around cv.wait().
    task<> wait(mutex& m) {
        m.unlock();
        co_await wait_awaiter(*this);
        co_await m.lock();
    }

    void notify_one() {
        while(auto* waiter = pop_waiter()) {
            if(resume_waiter(*waiter)) {
                break;
            }
        }
    }

    void notify_all() {
        drain_waiters([this](wait_node& waiter) { resume_waiter(waiter); });
    }
};

}  // namespace kota
