#include "kota/async/runtime/node.h"

#include <cassert>
#include <utility>

#include "../libuv.h"
#include "kota/async/io/loop.h"
#include "kota/async/runtime/sync.h"
#include "kota/async/runtime/walk.h"

namespace kota {

void dispatch(event_loop* loop, std::coroutine_handle<> handle) {
    if(!handle || handle == std::noop_coroutine()) {
        return;
    }
    if(loop) {
        loop->dispatch(handle);
    } else {
        handle.resume();
    }
}

void dispatch(event_loop* loop, async_node* node) {
    if(!node) {
        return;
    }
    if(loop) {
        loop->dispatch(node);
    } else {
        node->resume();
    }
}

void async_node::intercept_cancel() noexcept {
    policy = static_cast<Policy>(policy | InterceptCancel);
}

std::coroutine_handle<> aggregate_op::flush_deferred() noexcept {
    assert(deferred != Deferred::None && "flush_deferred requires a latched completion");
    assert(parent && "aggregate has deferred completion but no parent");

    phase = Phase::Settled;

    assert(parent->is_task_frame() && "aggregate parent must be a task");
    auto* p = static_cast<task_frame*>(parent);
    parent = nullptr;
    auto completion = deferred;
    deferred = Deferred::None;
    p->clear_child();

    switch(completion) {
        case Deferred::Resume: return p->handle();

        case Deferred::Cancel:
            if(policy & InterceptCancel) {
                state = Cancelled;
                return p->handle();
            }
            p->state = Cancelled;
            return p->finalize();

        case Deferred::Error: return p->handle();

        case Deferred::None: break;
    }

    std::abort();
}

void async_node::clear_child() noexcept {
    assert(is_task_frame() && "clear_child requires a task node");
    static_cast<task_frame*>(this)->set_child(nullptr);
}

event_loop* async_node::find_loop() const noexcept {
    for(auto* n = this; n; n = get_parent(*n)) {
        if(n->kind == NodeKind::Root) {
            return static_cast<const root_frame*>(n)->loop;
        }
    }
    return nullptr;
}

/// Recursively cancels this node and all of its descendants.
/// Idempotent: re-cancelling an already-terminal node is a no-op.
void async_node::cancel() {
    if(state == Cancelled || state == Failed || state == Finished) {
        return;
    }
    state = Cancelled;

    switch(kind) {
        case NodeKind::Root:
        case NodeKind::Task: {
            auto* self = static_cast<task_frame*>(this);
            if(self->child) {
                self->child->cancel();
            } else {
                auto* loop = find_loop();
                auto next = self->finalize();
                dispatch(loop, next);
            }
            break;
        }
        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter: {
            auto* self = static_cast<wait_node*>(this);
            auto* loop = self->find_loop();
            if(auto* res = self->resource) {
                res->remove(self);
            }
            auto* p = self->parent;
            self->parent = nullptr;
            assert(p && "wait_node cancelled without a parent");
            auto next = p->on_child_complete(*self);
            dispatch(loop, next);
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
                auto* loop = self->find_loop();
                auto next = self->flush_deferred();
                dispatch(loop, next);
            }
            break;
        }

        case NodeKind::SystemIO: {
            auto* self = static_cast<io_op*>(this);
            assert(self->action && "io_op cancelled without a cancellation action");
            self->action(self);
            break;
        }
    }
}

/// Resumes a task's coroutine, or finalizes it if already cancelled/failed.
void async_node::resume() {
    if(is_task_frame()) {
        if(is_cancelled() || is_failed()) {
            auto* loop = find_loop();
            auto next = finalize();
            dispatch(loop, next);
        } else {
            static_cast<task_frame*>(this)->handle().resume();
        }
    }
}

/// Called by libuv callbacks when an I/O operation completes.
/// Preserves Cancelled state if already set, then notifies the parent.
void io_op::complete() noexcept {
    if(state != Cancelled) {
        state = Finished;
    }
    auto* p = parent;
    auto* loop = find_loop();
    parent = nullptr;
    assert(p && "io_op completed without a linked parent");
    auto next = p->on_child_complete(*this);
    dispatch(loop, next);
}

/// Wires this node as a child of `parent_node`. For Task nodes, sets state
/// to Running and returns the coroutine handle (ready to resume).
/// For transient nodes (wait_node, io_op), records the parent
/// and returns noop_coroutine (resumed later by event/complete).
std::coroutine_handle<> async_node::attach(async_node& parent_node, std::source_location loc) {
    this->location = loc;
    if(parent_node.is_task_frame()) {
        static_cast<task_frame*>(&parent_node)->child = this;
    }

    switch(this->kind) {
        case NodeKind::Root:
        case NodeKind::Task: {
            auto self = static_cast<task_frame*>(this);
            self->state = Running;
            self->parent = &parent_node;
            return self->handle();
        }

        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter: {
            auto self = static_cast<wait_node*>(this);
            self->parent = &parent_node;
            return std::noop_coroutine();
        }
        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::TaskGroup: break;
        case NodeKind::SystemIO: {
            auto self = static_cast<io_op*>(this);
            self->parent = &parent_node;
            return std::noop_coroutine();
        }
    }

    std::abort();
}

/// Called when a task reaches final_suspend (Finished, Cancelled, or Failed).
/// For root tasks with no parent, destroys the coroutine frame.
/// Otherwise, notifies the parent via on_child_complete and clears the
/// parent link so that a subsequent finalize (e.g. from a deferred
/// dispatch after cancellation) is a safe no-op.
std::coroutine_handle<> async_node::finalize() {
    switch(kind) {
        case NodeKind::Root:
        case NodeKind::Task: {
            auto self = static_cast<task_frame*>(this);
            if(!self->parent) {
                if(self->root) {
                    if(!defer_frame_destroy(self->handle())) {
                        self->handle().destroy();
                    }
                }
                return std::noop_coroutine();
            }

            auto* p = self->parent;
            self->parent = nullptr;
            return p->on_child_complete(*self);
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
///   - Failed child (exception or structured error): cancels all siblings, resumes parent.
///   - WhenAny completion: records winner, cancels siblings, resumes parent.
///   - WhenAll completion: increments counter, resumes parent when all done.
std::coroutine_handle<> async_node::on_child_complete(async_node& child) {
    assert(&child != this && "invalid parameter!");

    switch(kind) {
        case NodeKind::Root:
        case NodeKind::Task: {
            auto self = static_cast<task_frame*>(this);

            if(child.state == Cancelled) {
                if(child.policy & InterceptCancel) {
                    self->child = nullptr;
                    return self->handle();
                }

                self->child = nullptr;
                self->state = Cancelled;
                return self->finalize();
            }

            self->child = nullptr;
            // If the child task has an error hook (set by or_fail_task_await),
            // let the hook handle error propagation instead of resuming normally.
            if(child.state == Failed && child.kind == NodeKind::Task) {
                auto* child_task = static_cast<task_frame*>(&child);
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
            } else if(self->kind == NodeKind::WhenAny && self->winner == aggregate_op::npos &&
                      self->deferred == aggregate_op::Deferred::None) {
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
                return self->flush_deferred();
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

            if(!self->parent) {
                return std::noop_coroutine();
            }

            const bool deferring = self->is_deferring();
            self->phase = aggregate_op::Phase::Settled;
            self->defer_resume();
            if(deferring) {
                return std::noop_coroutine();
            }
            return self->flush_deferred();
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
