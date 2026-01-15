#pragma once

#include <cstdlib>
#include <expected>

#include "frame.h"

namespace eventide {

struct cancellation_t {};

template <typename T>
class task;

constexpr inline cancellation_t cancellation_token;

template <typename T>
constexpr bool is_cancellable_v = false;

template <typename T>
constexpr bool is_cancellable_v<std::expected<T, cancellation_t>> = true;

template <typename T>
struct promise_result {
    std::conditional_t<is_cancellable_v<T>, T, std::expected<T, cancellation_t>> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }

    void return_value(cancellation_t) {
        value.emplace(std::unexpected(cancellation_t{}));
    }
};

template <>
struct promise_result<void> {
    void return_void() noexcept {}
};

namespace detail {

struct final {
    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> current) noexcept {
        return current.promise().finish();
    }

    void await_resume() noexcept {}
};

template <typename T, typename P>
struct task {
    std::coroutine_handle<P> callee;

    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> caller) noexcept {
        return callee.promise().suspend(caller.promise());
    }

    T await_resume() noexcept {
        if constexpr(!std::is_void_v<T>) {
            assert(handle.promise().value.has_value() && "await_resume: value not set");
            return std::move(*callee.promise().value);
        }
    }
};

}  // namespace detail

template <typename T>
struct promise_object : async_frame, promise_result<T> {
    auto handle() {
        return std::coroutine_handle<promise_object>::from_promise(*this);
    }

    auto initial_suspend() const noexcept {
        return std::suspend_always();
    }

    auto final_suspend() const noexcept {
        return detail::final();
    }

    auto get_return_object() {
        return task<T>(handle());
    }

    void unhandled_exception() {
        std::abort();
    }

    promise_object(std::source_location location = std::source_location::current()) {
        this->address = handle().address();
        this->location = location;
    }
};

template <typename T = void>
class task {
public:
    using promise_type = promise_object<T>;

    using coroutine_handle = std::coroutine_handle<promise_type>;

public:
    task() = default;

    explicit task(coroutine_handle h) noexcept : h(h) {}

    task(const task&) = delete;

    task(task&& other) noexcept : h(other.h) {
        other.h = nullptr;
    }

    task& operator=(const task&) = delete;

    auto operator co_await() const noexcept {
        return detail::task<T, promise_type>(h);
    }

    ~task() {
        if(h) {
            h.destroy();
        }
    }

private:
    coroutine_handle h;
};

}  // namespace eventide
