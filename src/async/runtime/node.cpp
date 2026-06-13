#include "kota/async/runtime/node.h"

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

void async_node::resume_and_drain(std::coroutine_handle<> handle) {
    static thread_local bool draining = false;

    assert(handle && "resume_and_drain called with null handle");
    const bool outermost = !draining;
    if(outermost) {
        draining = true;
    }

    handle.resume();
#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
    drain_pending_destroys();
#endif

    if(outermost && event_loop::has_current()) {
        event_loop::current().drain_deferred();
    }

    if(outermost) {
        draining = false;
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
    switch(completion) {
        case Deferred::Resume: p->set_child(p); return p->handle();

        case Deferred::Cancel:
            if(policy & InterceptCancel) {
                state = Cancelled;
                p->set_child(p);
                return p->handle();
            }
            p->set_child(nullptr);
            p->state = Cancelled;
            return p->finalize();

        case Deferred::Error: p->set_child(p); return p->handle();

        case Deferred::None: break;
    }

    std::abort();
}

void async_node::clear_child() noexcept {
    assert(kind == NodeKind::Task && "clear_child requires a task node");
    static_cast<task_frame*>(this)->set_child(nullptr);
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
            auto* self = static_cast<task_frame*>(this);
            if(self->child == self) {
                break;
            }
            if(self->child) {
                self->child->cancel();
            } else if(self->parent) {
                auto next = self->finalize();
                async_node::resume_and_drain(next);
            }
            break;
        }
        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter: {
            auto* self = static_cast<wait_node*>(this);
            if(auto* res = self->resource) {
                res->remove(self);
            }
            auto* p = self->parent;
            self->parent = nullptr;
            assert(p && "wait_node cancelled without a parent");
            auto next = p->on_child_complete(*self);
            async_node::resume_and_drain(next);
            break;
        }

        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::TaskGroup: {
            auto* self = static_cast<aggregate_op*>(this);
            self->cancel_siblings();

            // When InterceptCancel is set, state == Cancelled is sufficient
            // to signal external cancel — await_resume checks state, and
            // children completing through on_child_complete will record
            // first_cancel_child. Calling defer_cancel() here would overwrite
            // an existing deferred (e.g. Resume from a winner) and create a
            // Cancel outcome with no child attribution (first_cancel_child
            // == npos).
            //
            // Without InterceptCancel, defer_cancel() IS needed: the Cancel
            // path in flush_deferred propagates the cancel upward via
            // finalize, which won't happen on the Resume path.
            if(!(self->policy & InterceptCancel)) {
                self->defer_cancel();
            }

            if(self->is_deferring()) {
                break;
            }

            if(self->is_settled() || self->completed == self->total) {
                self->phase = aggregate_op::Phase::Settled;
                auto next = self->flush_deferred();
                async_node::resume_and_drain(next);
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
///
/// The cancel/fail path handles the case where a sync primitive deferred
/// this resume and cancellation arrived before the deferred tick fired.
/// If the child wait_node was resolved by resume_waiter (state == Finished),
/// its grant (e.g. a mutex lock) is abandoned before finalization.
void async_node::resume() {
    if(is_task_frame()) {
        auto* f = static_cast<task_frame*>(this);
        if(is_cancelled() || is_failed()) {
            auto* ch = f->child;
            if(ch && ch->is_wait_node() && ch->is_finished()) {
                auto* wn = static_cast<wait_node*>(ch);
                if(wn->abandon) {
                    auto fn = wn->abandon;
                    auto ctx = wn->abandon_context;
                    wn->abandon = nullptr;
                    wn->abandon_context = nullptr;
                    fn(ctx);
                }
            }
            f->set_child(nullptr);
            if(f->get_parent()) {
                auto next = f->finalize();
                resume_and_drain(next);
            }
            return;
        }
        f->set_child(f);
        f->handle().resume();
#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
        drain_pending_destroys();
#endif
    }
}

/// Called by libuv callbacks when an I/O operation completes.
/// Preserves Cancelled state if already set, then notifies the parent.
void io_op::complete() noexcept {
    if(state != Cancelled) {
        state = Finished;
    }
    auto* p = parent;
    parent = nullptr;
    assert(p && "io_op completed without a linked parent");
    auto next = p->on_child_complete(*this);
    async_node::resume_and_drain(next);
}

/// Wires this node as a child of `parent_node`. For Task nodes, sets state
/// to Running and returns the coroutine handle (ready to resume).
/// For transient nodes (wait_node, io_op), records the parent
/// and returns noop_coroutine (resumed later by event/complete).
std::coroutine_handle<> async_node::attach(async_node& parent_node, std::source_location loc) {
    this->location = loc;
    if(parent_node.kind == NodeKind::Task) {
        static_cast<task_frame*>(&parent_node)->child = this;
    }

    switch(this->kind) {
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
/// Otherwise, notifies the parent via on_child_complete.
std::coroutine_handle<> async_node::finalize() {
    switch(kind) {
        case NodeKind::Task: {
            auto self = static_cast<task_frame*>(this);
            if(!self->parent) {
                if(self->root) {
#if KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
                    pending_frame_destroys.push_back(self->handle());
#else
                    self->handle().destroy();
#endif
                }
                return std::noop_coroutine();
            }

            return self->parent->on_child_complete(*self);
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
        case NodeKind::Task: {
            auto self = static_cast<task_frame*>(this);

            if(child.state == Cancelled) {
                if(child.policy & InterceptCancel) {
                    self->set_child(self);
                    return self->handle();
                }

                self->set_child(nullptr);
                self->state = Cancelled;
                return self->finalize();
            }

            self->set_child(self);
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
                // Record first_cancel_child independently of the deferred
                // guard: a cancelled child can arrive after a winner has
                // already set deferred = Resume (e.g. in when_any), and an
                // external cancel may later cause the aggregate to settle
                // as Cancelled. await_resume needs a valid first_cancel_child
                // to extract the cancellation value.
                if(aggregate_catches_cancel && self->first_cancel_child == aggregate_op::npos) {
                    self->first_cancel_child = self->find_child_index(child);
                }
                if(self->deferred == aggregate_op::Deferred::None) {
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
