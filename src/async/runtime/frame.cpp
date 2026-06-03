#include "kota/async/runtime/frame.h"

#include <cassert>
#include <utility>
#include <vector>

#include "../libuv.h"
#include "kota/async/io/loop.h"
#include "kota/async/runtime/sync.h"

namespace kota {

namespace {

#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
thread_local std::vector<std::coroutine_handle<>> pending_frame_destroys;
#endif

#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
void drain_pending_destroys() {
    while(!pending_frame_destroys.empty()) {
        auto queued = std::move(pending_frame_destroys);
        pending_frame_destroys.clear();
        for(auto handle: queued) {
            assert(handle && "pending destroy queue contains a null handle");
            handle.destroy();
        }
    }
}
#endif

}  // namespace

void detail::resume_and_drain(std::coroutine_handle<> handle) {
    assert(handle && "resume_and_drain called with null handle");
    handle.resume();
#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
    drain_pending_destroys();
#endif
}

void async_node::intercept_cancel() noexcept {
    policy = static_cast<Policy>(policy | InterceptCancel);
}

std::coroutine_handle<> aggregate_op::deliver_deferred() noexcept {
    assert(deferred != Deferred::None && "deliver_deferred requires a latched completion");
    assert(awaiter && "aggregate has deferred completion but no awaiter");

    phase = Phase::Settled;

    assert(awaiter->is_standard_task() && "aggregate awaiter must be a task");
    auto* parent = static_cast<standard_task*>(awaiter);
    awaiter = nullptr;
    auto completion = deferred;
    deferred = Deferred::None;
    parent->clear_awaitee();

    switch(completion) {
        case Deferred::Resume: return parent->handle();

        case Deferred::Cancel:
            if(policy & InterceptCancel) {
                state = Cancelled;
                return parent->handle();
            }
            parent->state = Cancelled;
            return parent->final_transition();

        case Deferred::Error: return parent->handle();

        case Deferred::None: break;
    }

    std::abort();
}

void async_node::clear_awaitee() noexcept {
    assert(kind == NodeKind::Task && "clear_awaitee requires a task node");
    static_cast<standard_task*>(this)->set_awaitee(nullptr);
}

/// Recursively cancels this node and all of its descendants.
/// Idempotent: re-cancelling an already-terminal node is a no-op.
void async_node::cancel() {
    if(state == Cancelled || state == Failed || state == Finished) {
        return;
    }
    state = Cancelled;

    switch(kind) {
        case NodeKind::Task: {
            auto* self = static_cast<standard_task*>(this);
            if(self->awaitee) {
                self->awaitee->cancel();
            } else if(self->awaiter) {
                auto next = self->final_transition();
                detail::resume_and_drain(next);
            }
            break;
        }
        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter: {
            auto* self = static_cast<waiter_link*>(this);
            if(auto* res = self->resource) {
                res->remove(self);
            }
            auto* parent = self->awaiter;
            self->awaiter = nullptr;
            assert(parent && "waiter_link cancelled without an awaiter");
            auto next = parent->handle_subtask_result(*self);
            detail::resume_and_drain(next);
            break;
        }

        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::TaskGroup: {
            auto* self = static_cast<aggregate_op*>(this);
            self->cancel_siblings();
            self->defer_cancel();

            if(self->is_deferring()) {
                break;
            }

            if(self->is_settled() || self->completed == self->total) {
                self->phase = aggregate_op::Phase::Settled;
                auto next = self->deliver_deferred();
                detail::resume_and_drain(next);
            }
            break;
        }

        case NodeKind::SystemIO: {
            auto* self = static_cast<system_op*>(this);
            assert(self->action && "system_op cancelled without a cancellation action");
            self->action(self);
            break;
        }
    }
}

/// Resumes a standard task's coroutine, unless it has been cancelled.
void async_node::resume() {
    if(is_standard_task()) {
        if(!is_cancelled() && !is_failed()) {
            static_cast<standard_task*>(this)->handle().resume();
#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
            drain_pending_destroys();
#endif
        }
    }
}

/// Called by libuv callbacks when an I/O operation completes.
/// Preserves Cancelled state if already set, then notifies the parent.
void system_op::complete() noexcept {
    if(state != Cancelled) {
        state = Finished;
    }
    auto* parent = awaiter;
    awaiter = nullptr;
    assert(parent && "system_op completed without a linked parent");
    auto next = parent->handle_subtask_result(*this);
    detail::resume_and_drain(next);
}

/// Wires this node as a child of `awaiter`. For Task nodes, sets state
/// to Running and returns the coroutine handle (ready to resume).
/// For transient nodes (waiter_link, system_op), records the awaiter
/// and returns noop_coroutine (resumed later by event/complete).
std::coroutine_handle<> async_node::link_continuation(async_node& awaiter,
                                                      std::source_location loc) {
    this->location = loc;
    if(awaiter.kind == NodeKind::Task) {
        auto* p = static_cast<standard_task*>(&awaiter);
        p->awaitee = this;
    }

    switch(this->kind) {
        case NodeKind::Task: {
            auto self = static_cast<standard_task*>(this);
            self->state = Running;
            self->awaiter = &awaiter;
            return self->handle();
        }

        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter: {
            auto self = static_cast<waiter_link*>(this);
            self->awaiter = &awaiter;
            return std::noop_coroutine();
        }
        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::TaskGroup: break;
        case NodeKind::SystemIO: {
            auto self = static_cast<system_op*>(this);
            self->awaiter = &awaiter;
            return std::noop_coroutine();
        }
    }

    std::abort();
}

/// Called when a task reaches final_suspend (Finished, Cancelled, or Failed).
/// For root tasks with no awaiter, destroys the coroutine frame.
/// Otherwise, notifies the parent via handle_subtask_result.
std::coroutine_handle<> async_node::final_transition() {
    switch(kind) {
        case NodeKind::Task: {
            auto p = static_cast<standard_task*>(this);
            if(!p->awaiter) {
                if(p->root) {
#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
                    pending_frame_destroys.push_back(p->handle());
#else
                    p->handle().destroy();
#endif
                }
                return std::noop_coroutine();
            }

            return p->awaiter->handle_subtask_result(*p);
        }

        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter:
        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::TaskGroup:
        case NodeKind::SystemIO: break;
    }

    std::abort();
}

/// Dispatches a child's completion to its parent node.
///
/// For Task parents: resumes the coroutine normally for Finished/Failed,
///   or propagates cancellation upward.
/// For Aggregate parents (when_all/when_any/scope):
///   - Cancellation: cancels all siblings, propagates upward.
///   - Failed child (exception or structured error): cancels all siblings, resumes awaiter.
///   - WhenAny completion: records winner, cancels siblings, resumes awaiter.
///   - WhenAll completion: increments counter, resumes awaiter when all done.
std::coroutine_handle<> async_node::handle_subtask_result(async_node& child) {
    assert(&child != this && "invalid parameter!");

    switch(kind) {
        case NodeKind::Task: {
            auto self = static_cast<standard_task*>(this);

            if(child.state == Cancelled) {
                if(child.policy & InterceptCancel) {
                    self->awaitee = nullptr;
                    return self->handle();
                }

                self->awaitee = nullptr;
                self->state = Cancelled;
                return self->final_transition();
            }

            // Finished or Failed: resume parent normally.
            // await_resume handles exceptions (rethrow_if_exception)
            // and errors (explicit return value inspection).
            self->awaitee = nullptr;
            // If the child task has an error hook (set by or_fail_task_await),
            // let the hook handle error propagation instead of resuming normally.
            if(child.state == Failed && child.kind == NodeKind::Task) {
                auto* child_task = static_cast<standard_task*>(&child);
                if(auto propagate = child_task->get_error_hook()) {
                    return propagate(child, *self);
                }
            }
            return self->handle();
        }

        case NodeKind::WhenAll:
        case NodeKind::WhenAny: {
            auto self = static_cast<aggregate_op*>(this);
            assert(!self->is_settled() && "aggregate received child completion after settling");

            const bool aggregate_catches_cancel = self->policy & InterceptCancel;
            const bool cancelled = child.state == Cancelled &&
                                   (aggregate_catches_cancel || !(child.policy & InterceptCancel));
            const bool failed = child.state == Failed;

            bool trigger_cancel = false;

            if(failed) {
                const bool first_error = self->first_error_child == aggregate_op::npos;
                self->defer_error();
                if(first_error) {
                    self->first_error_child = self->find_child_index(child);
                    if(child.propagated_exception) {
                        self->propagated_exception = child.propagated_exception;
                    }
                    trigger_cancel = true;
                }
            } else if(cancelled) {
                if(self->deferred == aggregate_op::Deferred::None) {
                    if(aggregate_catches_cancel) {
                        self->first_cancel_child = self->find_child_index(child);
                    }
                    self->defer_cancel();
                    trigger_cancel = true;
                }
            } else if(self->kind == NodeKind::WhenAny && self->winner == aggregate_op::npos
                      && self->deferred == aggregate_op::Deferred::None) {
                self->winner = self->find_child_index(child);
                self->defer_resume();
                trigger_cancel = true;
            }

            if(trigger_cancel) {
                self->cancel_siblings(&child);
            }

            self->completed += 1;
            assert(self->completed <= self->total &&
                   "aggregate completed more children than it owns");

            if(self->completed == self->total) {
                const bool deferring = self->is_deferring();
                self->phase = aggregate_op::Phase::Settled;
                if(self->deferred == aggregate_op::Deferred::None) {
                    self->defer_resume();
                }
                if(deferring) {
                    return std::noop_coroutine();
                }
                return self->deliver_deferred();
            }

            return std::noop_coroutine();
        }

        case NodeKind::TaskGroup: {
            auto* self = static_cast<aggregate_op*>(this);

            assert(!self->is_settled() && "task_group received child completion after settling");

            self->completed += 1;
            assert(self->completed <= self->total &&
                   "task_group completed more children than it owns");

            if((child.state == Failed || child.state == Cancelled) &&
               self->deferred == aggregate_op::Deferred::None &&
               self->phase == aggregate_op::Phase::Open) {
                self->defer_resume();
                if(self->completed < self->total) {
                    self->cancel_siblings(&child);
                }
            }

            if(self->completed != self->total) {
                return std::noop_coroutine();
            }

            if(!self->awaiter) {
                return std::noop_coroutine();
            }

            const bool deferring = self->is_deferring();
            self->phase = aggregate_op::Phase::Settled;
            self->defer_resume();
            if(deferring) {
                return std::noop_coroutine();
            }
            return self->deliver_deferred();
        }

        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter:
        case NodeKind::SystemIO:
        default: {
            std::abort();
        }
    }
}

}  // namespace kota
