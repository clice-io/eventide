#pragma once

#include <algorithm>
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <source_location>
#include <vector>

#include "kota/support/config.h"

namespace kota {

class sync_primitive;

/// Type-erased base for all coroutine-related nodes in the task tree.
///
/// This hierarchy models awaitable runtime entities only.
/// Shared sync resources (mutex/event/semaphore/cv) live outside it and are
/// referenced by wait_node nodes while a task is blocked on them.
class async_node {
public:
    enum class NodeKind : std::uint8_t {
        Task,

        /// Wait queue entries — wait_node subclasses.
        /// Semaphore and CV reuse EventWaiter (identical cancel semantics).
        MutexWaiter,
        EventWaiter,

        /// Aggregate operations — when_all / when_any / task_group.
        WhenAll,
        WhenAny,
        TaskGroup,

        /// Pending libuv I/O — timers, signals, fs, network, etc.
        SystemIO,
    };

    enum Policy : uint8_t {
        None = 0,
        /// Reserved for future use.
        ExplicitCancel = 1 << 0,
        /// When set, cancellation of this node does NOT fail upward.
        /// The parent resumes normally and can inspect the cancelled state.
        /// Used by catch_cancel() and with_token().
        InterceptCancel = 1 << 1,
    };

    enum State : uint8_t {
        Pending,
        Running,
        Cancelled,
        Finished,
        Failed,
    };

    const NodeKind kind;

    Policy policy = None;

    State state = Pending;

    std::source_location location;

    bool is_task_frame() const noexcept {
        return kind == NodeKind::Task;
    }

    bool is_wait_node() const noexcept {
        return NodeKind::MutexWaiter <= kind && kind <= NodeKind::EventWaiter;
    }

    bool is_aggregate_op() const noexcept {
        return NodeKind::WhenAll <= kind && kind <= NodeKind::TaskGroup;
    }

    bool is_finished() const noexcept {
        return state == Finished;
    }

    bool is_cancelled() const noexcept {
        return state == Cancelled;
    }

    bool is_failed() const noexcept {
        return state == Failed;
    }

    // Keep this out-of-line. clang -O3 miscompiles direct promise policy writes in
    // coroutine return-object conversions, which can drop InterceptCancel. See also
    // https://github.com/llvm/llvm-project/issues/105595. Fixed in clang 21.
    void intercept_cancel() noexcept;

    /// If this node is a task, clear its child pointer.
    void clear_child() noexcept;

    void cancel();

    void resume();

    std::coroutine_handle<> attach(async_node& parent, std::source_location location);

    std::coroutine_handle<> finalize();

    std::coroutine_handle<> on_child_complete(async_node& child);

    static void resume_and_drain(std::coroutine_handle<> handle);

protected:
    explicit async_node(NodeKind k) : kind(k) {}

public:
    std::exception_ptr propagated_exception;
};

class task_frame : public async_node {
protected:
    friend class async_node;

    explicit task_frame() : async_node(NodeKind::Task) {}

public:
    bool root = false;

    /// Optional hook invoked when a child task fails, allowing the parent to
    /// intercept the error before normal resumption. Used by or_fail_task_await
    /// to propagate errors directly without resuming the parent coroutine.
    using error_hook = std::coroutine_handle<> (*)(async_node& child, async_node& parent);

    std::coroutine_handle<> handle() {
        return std::coroutine_handle<>::from_address(address);
    }

    bool has_child() const noexcept {
        return child != nullptr;
    }

    void set_child(async_node* node) noexcept {
        child = node;
    }

    void set_error_hook(error_hook fn) noexcept {
        error_hook_fn = fn;
    }

    error_hook get_error_hook() const noexcept {
        return error_hook_fn;
    }

    void clear_error_hook() noexcept {
        error_hook_fn = nullptr;
    }

    const async_node* get_parent() const noexcept {
        return parent;
    }

    const async_node* get_child() const noexcept {
        return child;
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

private:
    async_node* parent = nullptr;

    async_node* child = nullptr;

    error_hook error_hook_fn = nullptr;
};

class wait_node : public async_node {
public:
    friend class async_node;
    friend class sync_primitive;

    explicit wait_node(NodeKind k) : async_node(k) {}

    const async_node* get_parent() const noexcept {
        return parent;
    }

    const sync_primitive* get_resource() const noexcept {
        return resource;
    }

    const wait_node* get_next() const noexcept {
        return next;
    }

protected:
    using abandon_fn = void (*)(void*) noexcept;

    /// The sync_primitive this waiter is queued on (nullptr if not queued).
    sync_primitive* resource = nullptr;

    /// Captures which wait-queue generation this waiter joined.
    ///
    /// `event::interrupt()` must only cancel the waiters that were already
    /// present when the interrupt began. The tricky part is that cancelling one
    /// waiter resumes arbitrary user code synchronously, and that code may
    /// immediately enqueue a fresh waiter on the same resource before
    /// interrupt() continues. Tagging each waiter with the generation observed
    /// at insertion time lets interrupt() stop once it reaches a waiter that
    /// was added by a later, re-entrant wait.
    std::size_t generation = 0;

    /// Intrusive doubly-linked list pointers for the sync_primitive's wait queue.
    wait_node* prev = nullptr;
    wait_node* next = nullptr;

    async_node* parent = nullptr;

    void* abandon_context = nullptr;

    abandon_fn abandon = nullptr;
};

/// Base for when_all / when_any.
///
/// Uses a two-phase protocol in await_suspend:
///   1. Arming: link all children, then resume them. During this phase,
///      synchronous child completions are deferred instead of directly
///      resuming the parent (to avoid use-after-resume).
///   2. Post-arm: deliver any deferred completion once it is safe.
///
/// Aggregate state is tracked explicitly:
///   - `phase` controls whether child callbacks must be deferred.
///   - `deferred` latches the completion to deliver once deferral ends.
class aggregate_op : public async_node {
protected:
    friend class async_node;

    explicit aggregate_op(NodeKind k) : async_node(k) {}

public:
    const async_node* get_parent() const noexcept {
        return parent;
    }

    const std::vector<async_node*>& get_children() const noexcept {
        return children;
    }

protected:
    enum class Phase : std::uint8_t {
        /// Normal operating state after arming completes.
        /// Child callbacks may settle the aggregate immediately.
        Open,

        /// await_suspend is still linking/resuming children.
        /// Any child completion observed here must be deferred until
        /// await_suspend returns to avoid resuming the parent re-entrantly.
        Arming,

        /// The aggregate itself is propagating cancellation to children.
        /// Child callbacks can re-enter while this walk is in progress, so
        /// completion is deferred until the cancel cascade finishes.
        Cancelling,

        /// Final outcome has been chosen and further child callbacks are ignored.
        Settled,
    };

    enum class Deferred : std::uint8_t {
        /// No deferred completion is waiting to be delivered.
        None,

        Resume,

        Cancel,

        /// Error outranks all other deferred outcomes.
        Error,
    };

    /// Sentinel value for when_any: no winner yet.
    constexpr static std::size_t npos = (std::numeric_limits<std::size_t>::max)();

    async_node* parent = nullptr;

    std::vector<async_node*> children;

    /// Number of children that have completed so far.
    std::size_t completed = 0;

    /// Total number of children expected to complete.
    std::size_t total = 0;

    /// Index of the first child to finish (when_any only).
    std::size_t winner = npos;

    /// Index of the first child to finish with a structured error or exception.
    std::size_t first_error_child = npos;

    /// Index of the first child to finish with cancellation.
    std::size_t first_cancel_child = npos;

    /// Runtime phase for this aggregate while it is awaiting children.
    Phase phase = Phase::Open;

    /// Completion latched while callbacks are being deferred.
    Deferred deferred = Deferred::None;

    bool is_settled() const noexcept {
        return phase == Phase::Settled;
    }

    bool is_deferring() const noexcept {
        return phase == Phase::Arming || phase == Phase::Cancelling;
    }

    /// Latch a normal completion, but preserve any stronger deferred signal
    /// (cancel/error) that may already have won.
    void defer_resume() noexcept {
        if(deferred == Deferred::None) {
            deferred = Deferred::Resume;
        }
    }

    /// Cancellation outranks a plain resume and is itself outranked only by error.
    void defer_cancel() noexcept {
        if(deferred != Deferred::Error) {
            deferred = Deferred::Cancel;
        }
    }

    /// Error outranks all other deferred outcomes.
    void defer_error() noexcept {
        deferred = Deferred::Error;
    }

    std::size_t find_child_index(const async_node& child) const {
        auto it = std::ranges::find(children, &child);
        assert(it != children.end() && "child not found in aggregate");
        if(it == children.end())
            std::abort();
        return static_cast<std::size_t>(it - children.begin());
    }

    void cancel_siblings(async_node* exclude = nullptr) {
        [[maybe_unused]] bool found = exclude == nullptr;
        auto saved = phase;
        phase = Phase::Cancelling;
        for(auto* child: children) {
            assert(child && "aggregate contains a null child");
            if(child == exclude) {
                found = true;
                continue;
            }
            child->cancel();
        }
        assert(found && "cancel_siblings exclude is not a child of this aggregate");
        if(phase == Phase::Cancelling) {
            phase = saved;
        }
    }

    /// Rethrows the propagated exception if one was captured from a failed child.
    void rethrow_if_propagated() {
#if KOTA_ENABLE_EXCEPTIONS
        if(propagated_exception) {
            std::rethrow_exception(propagated_exception);
        }
#endif
    }

    /// Deliver the latched completion to the aggregate parent once it is safe
    /// to resume or propagate out of the current callback stack.
    std::coroutine_handle<> flush_deferred() noexcept;

    std::coroutine_handle<> arm_and_resume(async_node& parent_node,
                                           std::source_location loc) noexcept {
        this->location = loc;

        assert(parent_node.is_task_frame() && "aggregate parent must be a task");
        static_cast<task_frame*>(&parent_node)->set_child(this);

        parent = &parent_node;
        completed = 0;
        winner = npos;
        first_error_child = npos;
        first_cancel_child = npos;
        phase = Phase::Arming;
        deferred = Deferred::None;
        propagated_exception = nullptr;
        state = Running;

        for(auto* child: children) {
            assert(child && "aggregate contains a null child");
            child->attach(*this, location);
        }

        for(auto* child: children) {
            assert(child && "aggregate contains a null child");
            child->resume();
            if(is_settled() || deferred != Deferred::None) {
                break;
            }
        }

        if(phase == Phase::Arming) {
            phase = Phase::Open;
        }

        assert(completed <= total && "aggregate completed more children than it owns");
        if(completed == total && deferred != Deferred::None) {
            return flush_deferred();
        }

        return std::noop_coroutine();
    }
};

class io_op : public async_node {
protected:
    friend class async_node;

    using on_cancel = void (*)(io_op* self);

    explicit io_op(NodeKind k = NodeKind::SystemIO) : async_node(k) {}

    /// Callback invoked when this operation is cancelled (e.g. to close a uv handle).
    on_cancel action = nullptr;

    async_node* parent = nullptr;

public:
    void complete() noexcept;

    const async_node* get_parent() const noexcept {
        return parent;
    }
};

}  // namespace kota
