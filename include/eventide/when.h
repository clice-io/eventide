#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "frame.h"

namespace eventide {

namespace detail {

template <typename Task>
async_node* node_from(Task& task) {
    return task.operator->();
}

template <typename Task>
auto take_result(Task& task) {
    return std::move(task).result();
}

}  // namespace detail

template <typename... Tasks>
class when_all : public aggregate_op {
public:
    template <typename... U>
    explicit when_all(U&&... tasks) :
        aggregate_op(async_node::NodeKind::WhenAll), tasks_(std::forward<U>(tasks)...) {}

    bool await_ready() const noexcept {
        return sizeof...(Tasks) == 0;
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> awaiter_handle,
                      std::source_location location = std::source_location::current()) noexcept {
        auto* awaiter_node = static_cast<async_node*>(&awaiter_handle.promise());
        if(awaiter_node->kind == async_node::NodeKind::Task) {
            static_cast<standard_task*>(awaiter_node)->set_awaitee(this);
        }

        awaiter = awaiter_node;
        completed = 0;
        total = sizeof...(Tasks);
        winner = npos;
        done = false;
        pending_resume = false;
        pending_cancel = false;
        arming = true;

        awaitees.clear();
        awaitees.reserve(total);
        add_awaitees(std::index_sequence_for<Tasks...>{});

        for(auto* child: awaitees) {
            if(child) {
                child->link_continuation(this, location);
            }
        }

        for(auto* child: awaitees) {
            if(child) {
                child->resume();
                if(pending_cancel) {
                    break;
                }
            }
        }

        arming = false;
        if(pending_resume && awaiter) {
            if(pending_cancel) {
                awaiter->state = Cancelled;
                return awaiter->final_transition();
            }

            return static_cast<stable_node*>(awaiter)->handle();
        }

        return std::noop_coroutine();
    }

    auto await_resume() {
        return collect(std::index_sequence_for<Tasks...>{});
    }

private:
    template <std::size_t... I>
    void add_awaitees(std::index_sequence<I...>) {
        (awaitees.push_back(detail::node_from(std::get<I>(tasks_))), ...);
    }

    template <std::size_t... I>
    auto collect(std::index_sequence<I...>) {
        return std::tuple(detail::take_result(std::get<I>(tasks_))...);
    }

    std::tuple<Tasks...> tasks_;
};

template <typename... Tasks>
class when_any : public aggregate_op {
public:
    template <typename... U>
    explicit when_any(U&&... tasks) :
        aggregate_op(async_node::NodeKind::WhenAny), tasks_(std::forward<U>(tasks)...) {}

    bool await_ready() const noexcept {
        return sizeof...(Tasks) == 0;
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> awaiter_handle,
                      std::source_location location = std::source_location::current()) noexcept {
        auto* awaiter_node = static_cast<async_node*>(&awaiter_handle.promise());
        if(awaiter_node->kind == async_node::NodeKind::Task) {
            static_cast<standard_task*>(awaiter_node)->set_awaitee(this);
        }

        awaiter = awaiter_node;
        completed = 0;
        total = sizeof...(Tasks);
        winner = npos;
        done = false;
        pending_resume = false;
        pending_cancel = false;
        arming = true;

        awaitees.clear();
        awaitees.reserve(total);
        add_awaitees(std::index_sequence_for<Tasks...>{});

        for(auto* child: awaitees) {
            if(child) {
                child->link_continuation(this, location);
            }
        }

        for(auto* child: awaitees) {
            if(child) {
                child->resume();
                if(done || pending_resume || pending_cancel) {
                    break;
                }
            }
        }

        arming = false;
        if(pending_resume && awaiter) {
            if(pending_cancel) {
                awaiter->state = Cancelled;
                return awaiter->final_transition();
            }

            return static_cast<stable_node*>(awaiter)->handle();
        }

        return std::noop_coroutine();
    }

    std::size_t await_resume() const noexcept {
        return winner;
    }

private:
    template <std::size_t... I>
    void add_awaitees(std::index_sequence<I...>) {
        (awaitees.push_back(detail::node_from(std::get<I>(tasks_))), ...);
    }

    std::tuple<Tasks...> tasks_;
};

template <typename... Tasks>
when_all(Tasks&&...) -> when_all<std::decay_t<Tasks>...>;

template <typename... Tasks>
when_any(Tasks&&...) -> when_any<std::decay_t<Tasks>...>;

}  // namespace eventide
