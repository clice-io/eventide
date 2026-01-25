#pragma once

#include <cassert>
#include <cstdlib>
#include <expected>
#include <optional>
#include <vector>

#include "frame.h"

namespace eventide {

struct cancellation_t {};

template <typename T>
constexpr bool is_cancellation_t = false;

template <typename T>
constexpr bool is_cancellation_t<std::expected<T, cancellation_t>> = true;

template <typename T>
class task;

template <typename T>
using ctask = task<std::expected<T, cancellation_t>>;

template <typename T>
struct promise_result {
    std::optional<T> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }
};

template <typename T>
struct promise_result<std::expected<T, cancellation_t>> {
    std::optional<T> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }

    void return_value(cancellation_t) {}
};

template <>
struct promise_result<std::expected<void, cancellation_t>> {
    void return_void() noexcept {}
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
            awaitee.h.promise().state = async_node::Running;
            awaitee.h.promise().location = location;
            return awaitee.h.promise().suspend(awaiter.promise());
        }

        T await_resume() noexcept {
            auto& promise = awaitee.h.promise();
            if(promise.state == async_node::Cancelled) {
                if constexpr(is_cancellation_t<T>) {
                    return std::unexpected(cancellation_t());
                } else {
                    /// Implicitly spread cancellation, never call this.
                    std::abort();
                }
            }

            if(promise.state == async_node::Finished) {
                if constexpr(is_cancellation_t<T>) {
                    if constexpr(!std::is_void_v<typename T::value_type>) {
                        assert(promise.value.has_value() && "await_resume: value not set");
                        return std::move(*awaitee.h.promise().value);
                    } else {
                        return {};
                    }
                } else {
                    if constexpr(!std::is_void_v<T>) {
                        assert(promise.value.has_value() && "await_resume: value not set");
                        return std::move(*awaitee.h.promise().value);
                    } else {
                        return;
                    }
                }
            }

            std::abort();
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

    auto result() {
        if constexpr(!std::is_void_v<T>) {
            return std::move(*h.promise().value);
        } else {
            return std::nullopt;
        }
    }

    async_node* operator->() {
        return &h.promise();
    }

    ctask<T> catch_cancel() {
        if constexpr(is_cancellation_t<T>) {
            static_assert(!is_cancellation_t<T>, "already explicit cancellation");
        }

        h.promise().policy = async_node::InterceptCancel;
        auto handle = h;
        h = nullptr;
        /// We make sure they have totally same layout, so do it here is fine even if
        /// sounds like undefined behavior.
        using coroutine_handle = ctask<T>::coroutine_handle;
        return ctask<T>(coroutine_handle::from_address(handle.address()));
    }

private:
    coroutine_handle h;
};

}  // namespace eventide
