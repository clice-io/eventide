#include "eventide/async/frame.h"

#include <print>
#include <vector>

namespace eventide {

static thread_local async_frame* current_frame = nullptr;

void async_frame::destroy() {
    current_frame = this;
    handle().destroy();
}

void async_frame::cancel(this Self& self) {
    auto* cur = &self;
    while(cur) {
        cur->set_state(State::Cancelled);

        auto* child = cur->callee.ptr();
        if(!child) {
            break;
        }

        if(child->is_explicit_cancel()) {
            break;
        }

        cur = child;
    }
}

std::coroutine_handle<> async_frame::continuation(this Self& self) {
    // -----------------------------------------------------------------
    // 1. Fast Path: Normal Execution
    // -----------------------------------------------------------------
    // If the task is not marked as cancelled, resume execution immediately.
    if(!self.is_cancelled()) {
        current_frame = &self;
        return self.handle();
    }

    // -----------------------------------------------------------------
    // 2. Interception Path (Attempt/Catch)
    // -----------------------------------------------------------------
    // The task is cancelled, but the caller has explicitly set the
    // `InterceptCancel` policy (e.g., via `attempt()`).
    //
    // Instead of being destroyed, the task is allowed to resume. It is
    // expected to check `is_cancelled()` internally and return a
    // "Cancelled Result" to the parent.
    if(self.caller->is_intercept_cancel()) {
        current_frame = &self;
        return self.handle();
    }

    // -----------------------------------------------------------------
    // 3. Chain Destruction Path (The Death Spiral)
    // -----------------------------------------------------------------
    // We reached here because:
    // 1. The task is cancelled.
    // 2. No policy intervened to stop the destruction.
    //
    // THE IRON RULE: If a child task dies abnormally (without returning a value),
    // the parent task must also die because its dependency is broken.
    //
    // This loop destroys the call stack bottom-up until it reaches the root
    // or a detached node (where caller is null).
    async_frame* cur = &self;
    while(cur) {
        // Save the parent pointer before destroying the current node.
        // Accessing `p->caller` after `p->destroy()` constitutes Use-After-Free.
        async_frame* parent = cur->caller.ptr();

        // Perform physical destruction.
        // Only frames marked `Disposable` (usually heap-allocated task promises)
        // own the coroutine handle and need explicit destruction.
        // Stack-allocated waiters (like event_awaiters) will be destroyed
        // naturally when the parent frame is destroyed.
        if(cur->is_disposable()) {
            cur->destroy();
        } else {
            // If we cannot destroy it, mark it as Finished to prevent
            // double-free or logic errors later.
            cur->set_state(State::Finished);
        }

        // Climb up the chain.
        cur = parent;

        // [CRITICAL LOGIC]
        // We do NOT check `parent->InterceptCancel` inside this loop.
        //
        // Reason: This is a "Broken Dependency" scenario. Since the child `p`
        // has been destroyed without returning a value, the `parent` is left
        // in a limbo state at `await_suspend`. It cannot proceed.
        // Therefore, the parent must also be destroyed, regardless of whether
        // it was marked cancelled or intercepted.
    }

    // The entire dependency chain has been destroyed.
    // Yield control back to the event loop.
    return std::noop_coroutine();
}

std::coroutine_handle<> async_frame::suspend(this Self& self, async_frame& caller) {
    caller.callee = &self;
    self.caller = &caller;
    return self.continuation();
}

std::coroutine_handle<> async_frame::finish(this Self& self) {
    /// Mark current coroutine as finished.
    self.set_state(State::Finished);

    std::coroutine_handle<> handle = std::noop_coroutine();

    /// In the final suspend point, this coroutine is already done.
    /// So try to resume the waiting coroutine if it exists.
    if(self.caller) {
        handle = self.caller->continuation();
        self.caller->callee = nullptr;
    }

    /// If this task is disposable, destroy the coroutine handle.
    /// Note that this has been destroyed, never try to access it.
    if(self.is_disposable()) {
        self.destroy();
    }

    /// Return handle for resuming, it is safe on stack.
    return handle;
}

void async_frame::stacktrace(std::source_location location) {
    std::vector<async_frame*> chain;
    for(auto* cur = this; cur; cur = cur->caller.ptr()) {
        chain.push_back(cur);
    }

    async_frame* highlight = this;
    if(current_frame) {
        for(auto* frame: chain) {
            if(frame == current_frame) {
                highlight = current_frame;
                break;
            }
        }
    }

    for(auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto& loc = (*it)->location;
        std::println("  {} [{}:{}:{}]",
                     loc.function_name(),
                     loc.file_name(),
                     loc.line(),
                     loc.column());
    }

    std::println("->{} [{}:{}:{}]",
                 location.function_name(),
                 location.file_name(),
                 location.line(),
                 location.column());
}

async_frame* async_frame::current() {
    return current_frame;
}

}  // namespace eventide
