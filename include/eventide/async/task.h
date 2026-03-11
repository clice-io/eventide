#pragma once

#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstdlib>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

#include "awaitable.h"
#include "error.h"
#include "frame.h"
#include "loop.h"
#include "outcome.h"
#include "eventide/common/meta.h"

namespace eventide {

// ============================================================================
// promise_result specializations — keyed on E and C, not on T
// ============================================================================

template <typename T, typename E, typename C>
struct promise_result;

/// E=void, C=void: plain task — stores optional<T>.
template <typename T>
struct promise_result<T, void, void> {
    std::optional<T> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }
};

template <>
struct promise_result<void, void, void> {
    void return_void() noexcept {}
};

/// E=void, C!=void (cancel-only): stores optional<T>.
/// Layout matches promise_result<T, void, void> — enables from_address
/// trick in catch_cancel().
template <typename T, typename C>
    requires (!std::is_void_v<C>)
struct promise_result<T, void, C> {
    std::optional<T> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }

    void return_value(outcome_cancel_t<C>) noexcept {}

    void return_value(C) noexcept {}
};

template <typename C>
    requires (!std::is_void_v<C>)
struct promise_result<void, void, C> {
    void return_void() noexcept {}
};

/// E!=void, C=void (error-only): stores optional<outcome<T, E, void>>.
template <typename T, typename E>
    requires (!std::is_void_v<E>)
struct promise_result<T, E, void> {
    std::optional<outcome<T, E, void>> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }
};

/// E!=void, C!=void (both channels): stores optional<outcome<T, E, void>>.
/// Layout matches promise_result<T, E, void> — enables from_address
/// trick in catch_cancel().
template <typename T, typename E, typename C>
    requires (!std::is_void_v<E>) && (!std::is_void_v<C>)
struct promise_result<T, E, C> {
    std::optional<outcome<T, E, void>> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        value.emplace(std::forward<U>(val));
    }
};

// ============================================================================
// promise_exception, transition_await, cancel(), fail()
// ============================================================================

struct promise_exception {
#ifdef __cpp_exceptions
    void unhandled_exception() noexcept {
        this->exception = std::current_exception();
    }

    void rethrow_if_exception() {
        if(this->exception) {
            std::rethrow_exception(this->exception);
        }
    }

protected:
    std::exception_ptr exception{nullptr};
#else
    void unhandled_exception() {
        std::abort();
    }

    void rethrow_if_exception() {}
#endif
};

struct transition_await {
    async_node::State state = async_node::Pending;

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) const noexcept {
        auto& promise = handle.promise();
        if(state == async_node::Finished) {
            if(promise.state == async_node::Cancelled || promise.state == async_node::Failed) {
                return promise.final_transition();
            }
            assert(promise.state == async_node::Running && "only running task could finish");
            promise.state = state;
        } else if(state == async_node::Cancelled) {
            promise.state = state;
        } else if(state == async_node::Failed) {
            promise.state = state;
        } else {
            assert(false && "unexpected task state");
        }
        return promise.final_transition();
    }

    void await_resume() const noexcept {}
};

inline auto cancel() {
    return transition_await(async_node::Cancelled);
}

inline auto fail() {
    return transition_await(async_node::Failed);
}

// ============================================================================
// task<T, E, C>
// ============================================================================

template <typename T, typename E, typename C>
class task {
public:
    friend class event_loop;

    struct promise_type;

    using coroutine_handle = std::coroutine_handle<promise_type>;

    struct promise_type : standard_task, promise_result<T, E, C>, promise_exception {
        auto handle() {
            return coroutine_handle::from_promise(*this);
        }

        auto initial_suspend() const noexcept {
            return std::suspend_always();
        }

        auto final_suspend() const noexcept {
            return transition_await(async_node::Finished);
        }

        auto get_return_object() {
            return task(handle());
        }

        promise_type() {
            this->address = handle().address();
            if constexpr(!std::is_void_v<C>) {
                this->policy = static_cast<Policy>(this->policy | InterceptCancel);
            }
            if constexpr(!std::is_void_v<E>) {
                this->policy = static_cast<Policy>(this->policy | InterceptError);
            }
        }
    };

    struct awaiter {
        task awaitee;

        bool await_ready() noexcept {
            return false;
        }

        template <typename Promise>
        auto await_suspend(
            std::coroutine_handle<Promise> awaiter,
            std::source_location location = std::source_location::current()) noexcept {
            return awaitee.h.promise().link_continuation(&awaiter.promise(), location);
        }

        auto await_resume() {
            auto& promise = awaitee.h.promise();
            promise.rethrow_if_exception();

            if constexpr(std::is_void_v<E> && std::is_void_v<C>) {
                // Plain task: return T or void
                if(promise.state != async_node::Finished) {
                    std::abort();
                }
                if constexpr(!std::is_void_v<T>) {
                    assert(promise.value.has_value() && "await_resume: value not set");
                    return std::move(*promise.value);
                }
            } else {
                using R = outcome<T, E, C>;

                if(promise.state == async_node::Cancelled) {
                    if constexpr(!std::is_void_v<C>) {
                        if(promise.has_outcome() && promise.get_outcome()->is_cancelled()) {
                            return R(outcome_cancelled(
                                std::move(*promise.get_outcome()->template as<C>())));
                        }
                        return R(outcome_cancelled(C{}));
                    } else {
                        std::abort();
                    }
                }

                if(promise.state == async_node::Failed) {
                    if constexpr(!std::is_void_v<E>) {
                        if(promise.has_outcome() && promise.get_outcome()->has_error()) {
                            return R(
                                outcome_error(std::move(*promise.get_outcome()->template as<E>())));
                        }
                        return R(outcome_error(E{}));
                    } else {
                        std::abort();
                    }
                }

                if(promise.state == async_node::Finished) {
                    if constexpr(std::is_void_v<E>) {
                        // Cancel-only: promise stores optional<T>
                        if constexpr(!std::is_void_v<T>) {
                            assert(promise.value.has_value());
                            return R(std::move(*promise.value));
                        } else {
                            return R();
                        }
                    } else if constexpr(std::is_void_v<C>) {
                        // Error-only: promise stores optional<outcome<T, E, void>>
                        assert(promise.value.has_value());
                        return std::move(*promise.value);
                    } else {
                        // Both channels: promise stores optional<outcome<T, E, void>>
                        // Convert to outcome<T, E, C>
                        assert(promise.value.has_value());
                        auto& stored = *promise.value;
                        if(stored.has_error()) {
                            return R(outcome_error(std::move(stored).error()));
                        }
                        if constexpr(!std::is_void_v<T>) {
                            return R(std::move(*stored));
                        } else {
                            return R();
                        }
                    }
                }

                std::abort();
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

    task& operator=(task&& other) noexcept {
        if(this != &other) {
            if(h) {
                h.destroy();
            }
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }

    ~task() {
        if(h) {
            h.destroy();
        }
    }

    auto result() {
        auto&& promise = h.promise();
        promise.rethrow_if_exception();
        if constexpr(requires { promise.value; }) {
            assert(promise.value.has_value() && "result() on empty return");
            return std::move(*promise.value);
        } else {
            return std::nullopt;
        }
    }

    auto value() {
        auto&& promise = h.promise();
        promise.rethrow_if_exception();
        if constexpr(requires { promise.value; }) {
            return std::move(promise.value);
        } else {
            return std::nullopt;
        }
    }

    void release() {
        this->h = nullptr;
    }

    async_node* operator->() {
        return &h.promise();
    }

    /// Adds cancellation interception: task<T, E, void> → task<T, E, cancellation>.
    /// Uses from_address: safe because adding C never changes the promise layout.
    auto catch_cancel() && {
        static_assert(std::is_void_v<C>, "already has cancel channel");

        h.promise().policy =
            static_cast<async_node::Policy>(h.promise().policy | async_node::InterceptCancel);
        auto handle = h;
        h = nullptr;
        using target = task<T, E, cancellation>;
        using target_handle = typename target::coroutine_handle;
        return target(target_handle::from_address(handle.address()));
    }

    /// Adds error interception: task<T, void, void> → task<T, error, void>.
    /// Uses a wrapper coroutine (different promise layout).
    auto catch_error() && {
        static_assert(std::is_void_v<E>, "already has error channel");
        static_assert(std::is_void_v<C>, "use catch_all() to add both channels");

        h.promise().policy =
            static_cast<async_node::Policy>(h.promise().policy | async_node::InterceptError);
        return catch_error_wrapper(std::move(*this));
    }

    /// Adds both interceptions: task<T, void, void> → task<T, error, cancellation>.
    /// Uses a wrapper coroutine (different promise layout).
    auto catch_all() && {
        static_assert(std::is_void_v<E> && std::is_void_v<C>,
                      "already has error or cancel channel");

        h.promise().policy = static_cast<async_node::Policy>(
            h.promise().policy | async_node::InterceptError | async_node::InterceptCancel);
        return catch_all_wrapper(std::move(*this));
    }

private:
    static task<T, error, void> catch_error_wrapper(task<T, void, void> inner) {
        if constexpr(std::is_void_v<T>) {
            co_await std::move(inner);
            co_return outcome_value();
        } else {
            co_return co_await std::move(inner);
        }
    }

    static task<T, error, cancellation> catch_all_wrapper(task<T, void, void> inner) {
        if constexpr(std::is_void_v<T>) {
            co_await std::move(inner);
            co_return outcome_value();
        } else {
            co_return co_await std::move(inner);
        }
    }

    coroutine_handle h;
};

namespace detail {

template <typename T>
constexpr inline bool is_task_v = is_specialization_of<task, std::remove_cvref_t<T>>;

template <typename T>
concept owned_awaitable = !std::is_lvalue_reference_v<T> && awaitable<T&&>;

template <typename T>
using normalized_await_result_t = await_result_t<std::remove_cvref_t<T>&&>;

template <typename T, typename = void>
struct normalized_task;

template <typename T>
struct normalized_task<T, std::enable_if_t<is_task_v<T>>> {
    using type = std::remove_cvref_t<T>;
};

template <typename T>
struct normalized_task<T, std::enable_if_t<!is_task_v<T> && awaitable<std::remove_cvref_t<T>&&>>> {
    using type = task<normalized_await_result_t<T>>;
};

template <typename T>
using normalized_task_t = typename normalized_task<T>::type;

template <typename T, typename E, typename C>
task<T, E, C> normalize_task(task<T, E, C>&& t) {
    return std::move(t);
}

template <typename Awaitable>
    requires (!is_task_v<Awaitable>) && owned_awaitable<Awaitable>
auto normalize_task_impl(std::remove_cvref_t<Awaitable> value)
    -> task<normalized_await_result_t<Awaitable>> {
    if constexpr(std::is_void_v<normalized_await_result_t<Awaitable>>) {
        co_await std::move(value);
        co_return;
    } else {
        co_return co_await std::move(value);
    }
}

template <typename Awaitable>
    requires (!is_task_v<Awaitable>) && owned_awaitable<Awaitable>
auto normalize_task(Awaitable&& input) -> task<normalized_await_result_t<Awaitable>> {
    return normalize_task_impl<Awaitable>(
        std::remove_cvref_t<Awaitable>(std::forward<Awaitable>(input)));
}

}  // namespace detail

}  // namespace eventide
