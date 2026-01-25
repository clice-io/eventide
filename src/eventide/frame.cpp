#include "eventide/frame.h"

#include <cassert>
#include <utility>

#include "eventide/loop.h"

namespace eventide {

static thread_local async_node* current_node = nullptr;

void async_node::cancel() {
    if(state == Cancelled) {
        return;
    }
    state = Cancelled;

    switch(kind) {
        case NodeKind::Task: {
            auto* self = static_cast<standard_task*>(this);
            if(self->awaitee) {
                self->awaitee->cancel();
            }
            break;
        }
        case NodeKind::SharedTask:
        case NodeKind::Mutex:
        case NodeKind::Event:
        case NodeKind::Semaphore:
        case NodeKind::ConditionVariable: {
            auto* self = static_cast<shared_resource*>(this);
            if(self->awaitee) {
                self->awaitee->cancel();
            }

            auto* cur = self->head;
            while(cur) {
                auto* next = cur->next;
                /// FIXME:
                if(cur->awaiter) {}
                cur = next;
            }

            self->head = nullptr;
            self->tail = nullptr;
            break;
        }

        case NodeKind::SharedFuture:
        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter: {
            auto* self = static_cast<waiter_link*>(this);
            if(auto* res = self->resource) {
                if(self->prev) {
                    self->prev->next = self->next;
                } else {
                    res->head = self->next;
                }

                if(self->next) {
                    self->next->prev = self->prev;
                } else {
                    res->tail = self->prev;
                }

                self->prev = nullptr;
                self->next = nullptr;
                self->resource = nullptr;
            }
            if(self->awaiter) {
                /// FIXME:
            }
            break;
        }

        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::Scope: {
            auto* self = static_cast<aggregate_op*>(this);
            for(auto* child: self->awaitees) {
                if(child) {
                    child->cancel();
                }
            }
            break;
        }

        case NodeKind::Sleep:
        case NodeKind::SocketRead:
        case NodeKind::SocketWrite: {
            break;
        }
    }
}

std::coroutine_handle<> async_node::continuation(async_node* parent) {
    switch(parent->kind) {
        case NodeKind::Task: {
            auto p = static_cast<standard_task*>(parent);

            if(this == p) {
                if(!p->awaiter) {
                    if(p->root) {
                        p->handle().destroy();
                    }
                    return std::noop_coroutine();
                }

                return p->continuation(p->awaiter);
            }

            if(this->state == Finished) {
                /// If parent is standard task, and we finished as normal.
                /// Just return its handle and resume its next await point.
                current_node = p;
                return p->handle();
            }

            if(this->state == Cancelled) {
                /// If this task was set intercepted cancel, it represents
                /// the parent will handle the cancellation explicitly rather
                /// than implicitly spread. Just resume as normal.
                if(this->policy == InterceptCancel) {
                    current_node = p;
                    return p->handle();
                }

                p->state = Cancelled;
                if(p->awaiter) {
                    /// Otherwise set it as cancel and implicitly cancel it.
                    return p->continuation(p->awaiter);
                } else {
                    /// Top level coroutine.
                    if(p->root) {
                        p->handle().destroy();
                    }
                    return std::noop_coroutine();
                }
            }

            std::abort();
        }

        case NodeKind::SharedTask: {
            auto p = static_cast<shared_resource*>(parent);

            /// If the parent is not this, we are not at the final suspend point.
            /// If this async node was finished, we just resume next awaiter point.
            if(this != p && this->state == Finished) {
                current_node = p;
                return p->handle();
            }

            /// Otherwise, we are at final suspend point or the awaiting task
            /// was canceled. We need to resume all watchers.
            auto loop = event_loop::current();
            auto cur = p->head;
            while(cur) {
                if(state == Cancelled) {
                    cur->state = Cancelled;
                }

                /// Note that even if the shared_task was cancelled, we still
                /// need to resume it. Because we requires it to explicitly
                /// handle cancellation result.
                loop->schedule(static_cast<async_node&>(*cur), p->location);
                cur = cur->next;
            }

            p->head = nullptr;
            p->tail = nullptr;
            p->awaitee = nullptr;
            return std::noop_coroutine();
        }

        case NodeKind::Mutex:
        case NodeKind::Event:
        case NodeKind::Semaphore:
        case NodeKind::ConditionVariable:
        case NodeKind::SharedFuture:
        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter:
        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::Scope:
        case NodeKind::Sleep:
        case NodeKind::SocketRead:
        case NodeKind::SocketWrite:
        default: {
            std::abort();
        }
    }
}

std::coroutine_handle<> async_node::suspend() {
    return continuation(this);
}

std::coroutine_handle<> async_node::suspend(async_node& awaiter) {
    if(is_standard_task()) {
        static_cast<standard_task*>(this)->awaiter = &awaiter;
        static_cast<standard_task*>(&awaiter)->awaitee = this;
    }

    /// FIXME: ?
    return static_cast<standard_task*>(this)->handle();
}

void async_node::resume() {
    if(is_stable_node()) {
        if(!is_cancelled()) {
            static_cast<stable_node*>(this)->handle().resume();
        }
    }
}

}  // namespace eventide
