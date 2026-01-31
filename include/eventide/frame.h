#pragma once

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <source_location>
#include <vector>

namespace eventide {

class stable_node;

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

    std::coroutine_handle<> link_continuation(async_node* awaiter, std::source_location location);

    std::coroutine_handle<> final_transition();

    std::coroutine_handle<> handle_subtask_result(async_node* parent);

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

public:
    void set_awaitee(async_node* node) noexcept {
        awaitee = node;
    }

private:
    /// The node that awaits this task currently, if it is empty,
    /// this task is a top level task. It was launched by eventloop.
    async_node* awaiter = nullptr;

    /// The node that this task awaits.
    async_node* awaitee = nullptr;
};

class shared_resource;

class waiter_link : public transient_node {
public:
    friend class async_node;
    friend class shared_resource;

    explicit waiter_link(NodeKind k) : transient_node(k) {}

protected:
    shared_resource* resource = nullptr;

    waiter_link* prev = nullptr;

    waiter_link* next = nullptr;

    async_node* awaiter = nullptr;
};

class shared_resource : public stable_node {
public:
    friend class async_node;

    explicit shared_resource(NodeKind k) : stable_node(k) {}

    void inc_ref() {
        ref_count += 1;
    }

    void dec_ref() {
        ref_count -= 1;
        if(ref_count == 0) {
            handle().destroy();
        }
    }

    void insert(waiter_link* link);

    void remove(waiter_link* link);

protected:
    bool has_waiters() const noexcept {
        return head != nullptr;
    }

    waiter_link* pop_waiter() noexcept {
        auto* link = head;
        if(link) {
            remove(link);
        }
        return link;
    }

    template <typename Fn>
    void drain_waiters(Fn&& fn) {
        auto* cur = head;
        while(cur) {
            auto* next = cur->next;
            remove(cur);
            fn(cur);
            cur = next;
        }
    }

    bool resume_waiter(waiter_link* link) noexcept {
        if(!link) {
            return false;
        }
        auto* awaiting = link->awaiter;
        link->awaiter = nullptr;
        if(!awaiting || awaiting->is_cancelled()) {
            return false;
        }
        awaiting->resume();
        return true;
    }

private:
    std::uint32_t ref_count = 0;

    waiter_link* head = nullptr;

    waiter_link* tail = nullptr;

    async_node* awaitee = nullptr;
};

class aggregate_op : public transient_node {
protected:
    friend class async_node;

    explicit aggregate_op(NodeKind k) : transient_node(k) {}

protected:
    constexpr static std::size_t npos = (std::numeric_limits<std::size_t>::max)();

    async_node* awaiter = nullptr;

    std::vector<async_node*> awaitees;

    std::size_t completed = 0;

    std::size_t total = 0;

    std::size_t winner = npos;

    bool done = false;

    bool arming = false;

    bool pending_resume = false;

    bool pending_cancel = false;
};

class system_op : public transient_node {
protected:
    friend class async_node;

    explicit system_op(NodeKind k) : transient_node(k) {}
};

struct transition_await {
    async_node::State state = async_node::Pending;

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) const noexcept {
        auto& promise = handle.promise();
        if(state == async_node::Finished) {
            assert(promise.state == async_node::Running && "only running task could finish");
            promise.state = state;
        } else if(state == async_node::Cancelled) {
            promise.state = state;
        } else {
            assert(false && "unexpected task state");
        }
        return promise.final_transition();
    }

    void await_resume() const noexcept {}
};

inline auto cancel() {
    return transition_await(async_node::Cancelled);
}

}  // namespace eventide
