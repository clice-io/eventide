#include "eventide/frame.h"

#include <cassert>
#include <utility>

#include "libuv.h"
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
            uv_cancel(nullptr);
            break;
        }
    }
}

void async_node::resume() {
    const auto kind_snapshot = kind;
    /// Task/SharedTask/SharedFuture ...
    if(is_stable_node()) {
        if(!is_cancelled()) {
            static_cast<stable_node*>(this)->handle().resume();
        }
    }

    if(kind_snapshot == NodeKind::SharedFuture) {
        auto self = static_cast<waiter_link*>(this);
        static_cast<stable_node*>(self->awaiter)->handle().resume();
    }
}

std::coroutine_handle<> async_node::link_continuation(async_node* awaiter,
                                                      std::source_location location) {
    this->location = location;
    if(awaiter->kind == NodeKind::Task) {
        auto p = static_cast<standard_task*>(awaiter);
        p->awaitee = this;
    } else if(awaiter->kind == NodeKind::SharedTask) {
        auto p = static_cast<standard_task*>(awaiter);
        p->awaitee = this;
    }

    switch(this->kind) {
        case NodeKind::Task: {
            auto self = static_cast<standard_task*>(this);
            self->state = Running;
            self->awaiter = awaiter;
            return self->handle();
        }

        case NodeKind::SharedTask: {
            /// we never await shared task directly.
            std::abort();
        }

        case NodeKind::Mutex:
        case NodeKind::Event:
        case NodeKind::Semaphore:
        case NodeKind::ConditionVariable: {
            /// TODO:
            std::abort();
        }

        case NodeKind::SharedFuture: {
            auto self = static_cast<waiter_link*>(this);
            self->awaiter = awaiter;
            return std::noop_coroutine();
        }

        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter: {
            auto self = static_cast<waiter_link*>(this);
            self->awaiter = awaiter;
            return std::noop_coroutine();
        }
        case NodeKind::WhenAll:
        case NodeKind::WhenAny:
        case NodeKind::Scope:
        case NodeKind::Sleep:
        case NodeKind::SocketRead:
        case NodeKind::SocketWrite: break;
    }

    std::abort();
}

std::coroutine_handle<> async_node::final_transition() {
    switch(kind) {
        case NodeKind::Task: {
            auto p = static_cast<standard_task*>(this);
            if(!p->awaiter) {
                if(p->root) {
                    p->handle().destroy();
                }
                return std::noop_coroutine();
            }

            return p->awaiter->handle_subtask_result(p);
        }

        case NodeKind::SharedTask: {
            auto p = static_cast<shared_resource*>(this);
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
        case NodeKind::SocketWrite: break;
    }

    std::abort();
}

std::coroutine_handle<> async_node::handle_subtask_result(async_node* child) {
    assert(child && child != this && "invalid parameter!");

    switch(kind) {
        case NodeKind::Task:
        case NodeKind::SharedTask: {
            auto self = static_cast<stable_node*>(this);

            if(child->state == Finished) {
                /// If this is standard task, and we finished as normal.
                /// Just return its handle and resume its next await point.
                current_node = self;
                return self->handle();
            }

            if(child->state == Cancelled) {
                /// If child task was set intercepted cancel, it represents
                /// the this will handle the cancellation explicitly rather
                /// than implicitly spread. Just resume as normal.
                if(child->policy == InterceptCancel) {
                    current_node = self;
                    return self->handle();
                }

                self->state = Cancelled;
                return self->final_transition();
            }

            std::abort();
        }

        case NodeKind::WhenAll:
        case NodeKind::WhenAny: {
            auto self = static_cast<aggregate_op*>(this);
            if(self->done) {
                return std::noop_coroutine();
            }

            const bool cancelled = child->state == Cancelled && child->policy != InterceptCancel;
            if(cancelled) {
                self->done = true;
                self->pending_cancel = true;

                for(auto* other: self->awaitees) {
                    if(other && other != child) {
                        other->cancel();
                    }
                }

                if(self->arming) {
                    self->pending_resume = true;
                    return std::noop_coroutine();
                }

                if(self->awaiter) {
                    self->awaiter->state = Cancelled;
                    return self->awaiter->final_transition();
                }

                return std::noop_coroutine();
            }

            if(self->kind == NodeKind::WhenAny) {
                if(self->winner == aggregate_op::npos) {
                    for(std::size_t i = 0; i < self->awaitees.size(); ++i) {
                        if(self->awaitees[i] == child) {
                            self->winner = i;
                            break;
                        }
                    }
                }

                self->done = true;
                for(auto* other: self->awaitees) {
                    if(other && other != child) {
                        other->cancel();
                    }
                }

                if(self->arming) {
                    self->pending_resume = true;
                    return std::noop_coroutine();
                }

                if(self->awaiter) {
                    return static_cast<stable_node*>(self->awaiter)->handle();
                }

                return std::noop_coroutine();
            }

            self->completed += 1;
            if(self->completed >= self->total) {
                self->done = true;
                if(self->arming) {
                    self->pending_resume = true;
                    return std::noop_coroutine();
                }

                if(self->awaiter) {
                    return static_cast<stable_node*>(self->awaiter)->handle();
                }
            }

            return std::noop_coroutine();
        }

        case NodeKind::Mutex:
        case NodeKind::Event:
        case NodeKind::Semaphore:
        case NodeKind::ConditionVariable:
        case NodeKind::SharedFuture:
        case NodeKind::MutexWaiter:
        case NodeKind::EventWaiter:
        case NodeKind::Scope:
        case NodeKind::Sleep:
        case NodeKind::SocketRead:
        case NodeKind::SocketWrite:
        default: {
            /// TODO:
            std::abort();
        }
    }
}

void shared_resource::insert(waiter_link* link) {
    assert(link && "insert: null waiter_link");
    assert(link->resource == nullptr && "insert: waiter_link already linked");
    assert(link->prev == nullptr && link->next == nullptr && "insert: waiter_link has links");

    link->resource = this;

    if(tail) {
        tail->next = link;
        link->prev = tail;
        tail = link;
    } else {
        head = link;
        tail = link;
    }
}

void shared_resource::remove(waiter_link* link) {
    assert(link && "remove: null waiter_link");
    assert(link->resource == this && "remove: waiter_link not owned by resource");

    if(link->prev) {
        link->prev->next = link->next;
    } else {
        head = link->next;
    }

    if(link->next) {
        link->next->prev = link->prev;
    } else {
        tail = link->prev;
    }

    link->prev = nullptr;
    link->next = nullptr;
    link->resource = nullptr;
}

}  // namespace eventide
