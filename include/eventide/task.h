#pragma once

#include <cassert>
#include <cstdlib>
#include <expected>
#include <vector>

#include "frame.h"

namespace eventide {

struct cancellation_t {};

template <typename T>
class task;

constexpr inline cancellation_t cancellation_token;

template <typename T>
using maybe = std::expected<T, cancellation_t>;

template <typename T>
constexpr bool is_cancellable_v = false;

template <typename T>
constexpr bool is_cancellable_v<maybe<T>> = true;

template <typename T>
struct promise_result {
    std::conditional_t<is_cancellable_v<T>, T, maybe<T>> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }

    void return_value(cancellation_t) {
        value = std::unexpected(cancellation_t());
    }
};

template <>
struct promise_result<void> {
    void return_void() noexcept {}
};

template <typename T = void>
class task {
public:
    friend class event_loop;

    struct promise_type;

    using coroutine_handle = std::coroutine_handle<promise_type>;

    struct promise_type : standard_task, promise_result<T> {
        auto handle() {
            return coroutine_handle::from_promise(*this);
        }

        auto initial_suspend() noexcept {
            return std::suspend_always();
        }

        auto final_suspend() const noexcept {
            return final_awaiter();
        }

        auto get_return_object() {
            return task<T>(handle());
        }

        void unhandled_exception() {
            std::abort();
        }

        promise_type() {
            this->address = handle().address();
        }
    };

    struct awaiter {
        task<T> awaitee;

        bool await_ready() noexcept {
            return false;
        }

        template <typename Promise>
        auto await_suspend(
            std::coroutine_handle<Promise> awaiter,
            std::source_location location = std::source_location::current()) noexcept {
            awaitee.h.promise().location = location;
            return awaitee.h.promise().suspend(awaiter.promise());
        }

        T await_resume() noexcept {
            if constexpr(!std::is_void_v<T>) {
                assert(awaitee.h.promise().value.has_value() && "await_resume: value not set");
                if constexpr(is_cancellable_v<T>) {
                    return std::move(awaitee.h.promise().value);
                } else {
                    return std::move(*awaitee.h.promise().value);
                }
            }
        }
    };

    auto operator co_await() && noexcept {
        return awaiter(std::move(*this));
    }

public:
    task() = default;

    explicit task(coroutine_handle h) noexcept : h(h) {}

    task(const task&) = delete;

    task(task&& other) noexcept : h(other.h) {
        other.h = nullptr;
    }

    task& operator=(const task&) = delete;

    ~task() {
        if(h) {
            h.destroy();
        }
    }

    T result() {
        return std::move(*h.promise().value);
    }

    async_node* operator->() {
        return &h.promise();
    }

    task<maybe<T>> catch_cancel() {
        /// auto handle = h;
        /// h = nullptr;
        /// using coroutine_handle = std::coroutine_handle<promise_object<maybe<T>>>;
        /// return task<maybe<T>>(coroutine_handle::from_address(handle.address()));
    }

private:
    coroutine_handle h;
};

}  // namespace eventide
