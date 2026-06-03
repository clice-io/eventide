#include "kota/async/runtime/sync.h"

#include <cassert>

namespace kota {

void sync_primitive::insert(wait_node* link) {
    assert(link && "insert: null wait_node");
    assert(link->resource == nullptr && "insert: wait_node already linked");
    assert(link->prev == nullptr && link->next == nullptr && "insert: wait_node has links");

    link->resource = this;
    // Snapshot semantics for interrupt() depend on each waiter remembering the
    // generation that was current when it joined the queue.
    link->generation = waiter_generation;

    if(tail) {
        tail->next = link;
        link->prev = tail;
        tail = link;
    } else {
        head = link;
        tail = link;
    }
}

void sync_primitive::remove(wait_node* link) {
    assert(link && "remove: null wait_node");
    assert(link->resource == this && "remove: wait_node not owned by resource");

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

bool sync_primitive::cancel_waiter(wait_node& link) noexcept {
    auto* awaiting = link.parent;
    link.parent = nullptr;
    assert(awaiting && "cancel_waiter: waiter has no parent");
    if(awaiting->is_cancelled()) {
        return false;
    }

    link.state = async_node::Cancelled;
    link.policy = static_cast<async_node::Policy>(link.policy | async_node::InterceptCancel);
    auto next = awaiting->on_child_complete(link);
    async_node::resume_and_drain(next);
    return true;
}

}  // namespace kota
