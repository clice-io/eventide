#pragma once

#include <coroutine>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <vector>

namespace eventide {

class async_node {
public:
    enum class NodeKind : std::uint8_t {
        Task,

        SharedTask,
        Mutex,
        Event,
        Semaphore,
        ConditionVariable,

        SharedFuture,
        MutexWaiter,
        EventWaiter,

        WhenAll,
        WhenAny,
        Scope,

        Sleep,
        SocketRead,
        SocketWrite,
    };

    enum Policy : uint8_t {
        None = 0,
        ExplicitCancel = 1 << 0,   // bit 0
        InterceptCancel = 1 << 1,  // bit 1
    };

    enum State : uint8_t {
        Pending,
        Running,
        Cancelled,
        Finished,
    };

    const NodeKind kind;

    Policy policy;

    State state = Pending;

    bool root = false;

    std::source_location location;

    static async_node* current() {
        return nullptr;
    }

    bool is_standard_task() const noexcept {
        return kind == NodeKind::Task;
    }

    bool is_shared_resource() const noexcept {
        return NodeKind::SharedTask <= kind && kind <= NodeKind::ConditionVariable;
    }

    bool is_waiter_link() const noexcept {
        return NodeKind::SharedTask <= kind && kind <= NodeKind::EventWaiter;
    }

    bool is_aggregate_op() const noexcept {
        return NodeKind::SharedFuture <= kind && kind <= NodeKind::Scope;
    }

    bool is_stable_node() const noexcept {
        return is_standard_task() || is_shared_resource();
    }

    bool is_transient_node() const noexcept {
        return !is_stable_node();
    }

    bool is_finished() const noexcept {
        return state == Finished;
    }

    bool is_cancelled() const noexcept {
        return state == Cancelled;
    }

    void cancel();

    void resume();

    std::coroutine_handle<> continuation(async_node* parent);

    std::coroutine_handle<> suspend();

    std::coroutine_handle<> suspend(async_node& awaiter);

protected:
    explicit async_node(NodeKind k) : kind(k) {}
};

class stable_node : public async_node {
protected:
    explicit stable_node(NodeKind k) : async_node(k) {}

public:
    std::coroutine_handle<> handle() {
        return std::coroutine_handle<>::from_address(address);
    }

protected:
    /// Stores the raw address of the coroutine frame (handle).
    ///
    /// Theoretically, this is redundant because the promise object is embedded
    /// within the coroutine frame. However, deriving the frame address from `this`
    /// (via `from_promise`) requires knowing the concrete Promise type to account
    /// for the opaque compiler overhead (e.g., resume/destroy function pointers)
    /// located before the promise.
    ///
    /// Since this base class is type-erased, we cannot calculate that offset dynamically
    /// and must explicitly cache the handle address here (costing 1 pointer size).
    void* address = nullptr;
};

class transient_node : public async_node {
protected:
    explicit transient_node(NodeKind k) : async_node(k) {}
};

class standard_task : public stable_node {
protected:
    friend class async_node;

    explicit standard_task() : stable_node(NodeKind::Task) {}

private:
    /// The node that awaits this task currently, if it is empty,
    /// this task is a top level task. It was launched by eventloop.
    async_node* awaiter = nullptr;

    /// The node that this task awaits.
    async_node* awaitee = nullptr;
};

class waiter_link;

class shared_resource : public stable_node {
protected:
    friend class async_node;

    explicit shared_resource(NodeKind k) : stable_node(k) {}

private:
    std::uint32_t ref_count = 0;

    waiter_link* head = nullptr;

    waiter_link* tail = nullptr;

    async_node* awaitee = nullptr;
};

class waiter_link : public transient_node {
protected:
    friend class async_node;

    explicit waiter_link(NodeKind k) : transient_node(k) {}

private:
    shared_resource* resource = nullptr;

    waiter_link* prev = nullptr;

    waiter_link* next = nullptr;

    async_node* awaiter = nullptr;
};

class aggregate_op : public transient_node {
protected:
    friend class async_node;

    explicit aggregate_op(NodeKind k) : transient_node(k) {}

private:
    async_node* awaiter;

    std::vector<async_node*> awaitees;
};

class system_op : public transient_node {
protected:
    friend class async_node;

    explicit system_op(NodeKind k) : transient_node(k) {}
};

struct final_awaiter {
    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) const noexcept {
        auto& promise = handle.promise();
        if(promise.state == async_node::Running) {
            promise.state = async_node::Finished;
        } else {
            std::terminate();
        }
        return handle.promise().suspend();
    }

    void await_resume() const noexcept {}
};

struct cancel_awaiter {
    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) const noexcept {
        auto& promise = handle.promise();
        promise.state = async_node::Cancelled;
        return handle.promise().suspend();
    }

    void await_resume() const noexcept {}
};

inline auto cancel() {
    return cancel_awaiter();
}

}  // namespace eventide
