#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "kota/support/memory.h"
#include "kota/support/small_vector.h"
#include "kota/async/runtime/node.h"
#include "kota/async/runtime/traits.h"
#include "kota/async/vocab/outcome.h"

namespace kota {

template <bool All, typename... Tasks>
class when_op : public aggregate_op {
    constexpr static bool capture_cancel =
        !std::is_void_v<detail::aggregated_cancel_t<detail::task_cancel_type_t<Tasks>...>>;

public:
    using error_type = detail::aggregated_channel_t<detail::task_error_type_t<Tasks>...>;
    using cancel_type = detail::aggregated_cancel_t<detail::task_cancel_type_t<Tasks>...>;

    using success_type =
        std::conditional_t<All,
                           std::tuple<detail::task_success_t<Tasks, capture_cancel>...>,
                           std::variant<detail::task_success_t<Tasks, capture_cancel>...>>;

    using result_type = detail::aggregate_result_t<success_type, error_type, cancel_type>;

    template <detail::awaitable... U>
    explicit when_op(U... asyncs) :
        aggregate_op(All ? async_node::NodeKind::WhenAll : async_node::NodeKind::WhenAny),
        tasks(detail::normalize_task(std::move(asyncs))...) {
        static_assert(All || sizeof...(Tasks) > 0, "when_any requires at least one task");
        if constexpr(!std::is_void_v<cancel_type>) {
            this->intercept_cancel();
        }
    }

    ~when_op() = default;

    bool await_ready() const noexcept {
        if constexpr(All) {
            return sizeof...(Tasks) == 0;
        } else {
            return false;
        }
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> parent_handle,
                      std::source_location location = std::source_location::current()) noexcept {
        children.clear();
        children.reserve(sizeof...(Tasks));
        std::apply([&](auto&... ts) { (children.push_back(detail::node_from(ts)), ...); }, tasks);
        return arm_and_resume(parent_handle.promise(), location);
    }

    auto await_resume() -> result_type {
        rethrow_if_propagated();

        // Errors outrank cancellation: settle() never reports Cancelled when a
        // child error was recorded, even if an external cancel raced it.
        if constexpr(!std::is_void_v<error_type>) {
            if(first_error_child != aggregate_op::npos) {
                auto error = detail::tuple_visit_at_return<error_type>(
                    first_error_child,
                    tasks,
                    [&](auto, auto& task) -> error_type {
                        using task_t = std::remove_reference_t<decltype(task)>;
                        if constexpr(!std::is_void_v<detail::task_error_type_t<task_t>>) {
                            auto result = detail::take_result(task);
                            assert(result.has_error());
                            return error_type(std::move(result).error());
                        } else {
                            assert(false && "error child must expose an error channel");
                            std::abort();
                        }
                    });
                return result_type(outcome_error(std::move(error)));
            }
        }

        if constexpr(!std::is_void_v<cancel_type>) {
            if(this->state == async_node::Cancelled) {
                assert(first_cancel_child != aggregate_op::npos &&
                       "InterceptCancel aggregate cancelled with no attributed child");
                auto cancel = detail::tuple_visit_at_return<cancel_type>(
                    first_cancel_child,
                    tasks,
                    [&](auto, auto& task) -> cancel_type {
                        using task_t = std::remove_reference_t<decltype(task)>;
                        if constexpr(std::is_void_v<detail::task_cancel_type_t<task_t>>) {
                            return cancel_type(cancellation{});
                        } else {
                            auto result = detail::take_result(task);
                            assert(result.is_cancelled());
                            return cancel_type(std::move(result).cancellation());
                        }
                    });
                return result_type(outcome_cancel(std::move(cancel)));
            }
        }

        auto success = collect_success();
        if constexpr(std::same_as<result_type, success_type>) {
            return std::move(success);
        } else {
            return result_type(std::move(success));
        }
    }

private:
    auto collect_success() -> success_type {
        if constexpr(All) {
            return [this]<std::size_t... I>(std::index_sequence<I...>) {
                return success_type(
                    detail::take_success_result<capture_cancel>(std::get<I>(tasks))...);
            }(std::index_sequence_for<Tasks...>{});
        } else {
            assert(winner != aggregate_op::npos && "when_any winner not set");
            return detail::tuple_visit_at_return<success_type>(
                winner,
                tasks,
                [&](auto I, auto& task) -> success_type {
                    return success_type(std::in_place_index<I.value>,
                                        detail::take_success_result<capture_cancel>(task));
                });
        }
    }

    std::tuple<Tasks...> tasks;
};

template <bool All, typename Task>
class when_op<All, detail::range_tasks<Task>> : public aggregate_op {
    constexpr static bool capture_cancel = !std::is_void_v<detail::task_cancel_type_t<Task>>;

public:
    using error_type = detail::task_error_type_t<Task>;
    using cancel_type = detail::task_cancel_type_t<Task>;

    using success_type =
        std::conditional_t<All,
                           small_vector<detail::task_success_t<Task, capture_cancel>>,
                           std::pair<std::size_t, detail::task_success_t<Task, capture_cancel>>>;

    using result_type = detail::aggregate_result_t<success_type, error_type, cancel_type>;

    template <detail::async_range Range>
        requires std::same_as<detail::normalized_range_task_t<Range>, Task>
    explicit when_op(Range range) :
        aggregate_op(All ? async_node::NodeKind::WhenAll : async_node::NodeKind::WhenAny) {
        if constexpr(std::ranges::sized_range<Range>) {
            tasks.reserve(std::ranges::size(range));
        }
        for(auto&& async: range) {
            tasks.emplace_back(detail::normalize_task(std::move(async)));
        }
        if constexpr(!All) {
            if(tasks.empty()) {
                detail::fail_empty_when_any_range();
            }
        }
        if constexpr(!std::is_void_v<cancel_type>) {
            this->intercept_cancel();
        }
    }

    ~when_op() = default;

    bool await_ready() const noexcept {
        if constexpr(All) {
            return tasks.empty();
        } else {
            return false;
        }
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> parent_handle,
                      std::source_location location = std::source_location::current()) noexcept {
        children.clear();
        children.reserve(tasks.size());
        for(auto& task: tasks) {
            children.push_back(detail::node_from(task));
        }
        return arm_and_resume(parent_handle.promise(), location);
    }

    auto await_resume() -> result_type {
        rethrow_if_propagated();

        // Errors outrank cancellation — see the variadic overload.
        if constexpr(!std::is_void_v<error_type>) {
            if(first_error_child != aggregate_op::npos) {
                auto result = detail::take_result(tasks[first_error_child]);
                assert(result.has_error());
                return result_type(outcome_error(error_type(std::move(result).error())));
            }
        }

        if constexpr(!std::is_void_v<cancel_type>) {
            if(this->state == async_node::Cancelled) {
                assert(first_cancel_child != aggregate_op::npos &&
                       "InterceptCancel aggregate cancelled with no attributed child");
                auto result = detail::take_result(tasks[first_cancel_child]);
                assert(result.is_cancelled());
                return result_type(outcome_cancel(std::move(result).cancellation()));
            }
        }

        auto success = collect_success();
        if constexpr(std::same_as<result_type, success_type>) {
            return std::move(success);
        } else {
            return result_type(std::move(success));
        }
    }

private:
    auto collect_success() -> success_type {
        if constexpr(All) {
            success_type results;
            results.reserve(tasks.size());
            for(auto& task: tasks) {
                results.emplace_back(detail::take_success_result<capture_cancel>(task));
            }
            return results;
        } else {
            assert(winner != aggregate_op::npos && "when_any winner not set");
            return success_type{winner, detail::take_success_result<capture_cancel>(tasks[winner])};
        }
    }

    small_vector<Task> tasks;
};

/// Awaits all tasks concurrently, collecting results into a tuple.
///
/// Variadic overload: accepts heterogeneous awaitables (tasks, semaphore acquires, etc.)
/// and returns `std::tuple<T...>` where each element is the result of the corresponding task.
/// Void tasks produce `std::nullopt_t` in the tuple.
///
/// Range overload: accepts a range of homogeneous tasks and returns `small_vector<T>`.
///
/// If any child task produces a structured error, the first error cancels all siblings
/// and the combinator returns `outcome<..., E, ...>` carrying that error.
/// If any child cancels (via `co_await cancel()` or an external token), cancellation
/// propagates to all siblings and the combinator returns the cancellation.
/// Errors take priority over cancellations.
///
/// All children are guaranteed to have completed before the aggregate returns
/// (structured completion).
///
/// Accepts any awaitable that satisfies the `awaitable` concept, including synchronous
/// awaiters like `semaphore::acquire_awaiter`.
template <typename... Tasks>
class when_all : private when_op<true, Tasks...> {
    using when_op<true, Tasks...>::when_op;

public:
    using when_op<true, Tasks...>::await_ready;
    using when_op<true, Tasks...>::await_suspend;
    using when_op<true, Tasks...>::await_resume;
};

/// Awaits multiple tasks concurrently, returning the first to complete.
///
/// Variadic overload: accepts heterogeneous awaitables and returns
/// `std::variant<T...>` where the active alternative corresponds to the winning task.
/// Void tasks produce `std::nullopt_t` in the variant.
///
/// Range overload: accepts a range of homogeneous tasks and returns
/// `std::pair<std::size_t, T>` where the first element is the index of the winner.
///
/// Once a winner is determined, all remaining tasks are cancelled.
/// If the first-to-complete task produces a structured error, the combinator
/// returns `outcome<..., E, ...>` carrying that error.
/// If the first-to-complete task cancels, cancellation propagates to the parent.
///
/// Requires at least one task; `when_any<>` is explicitly deleted.
///
/// All siblings are guaranteed to have completed before the aggregate returns
/// (structured completion).
///
/// Accepts any awaitable that satisfies the `awaitable` concept, including synchronous
/// awaiters like `semaphore::acquire_awaiter`.
template <typename... Tasks>
class when_any : private when_op<false, Tasks...> {
    using when_op<false, Tasks...>::when_op;

public:
    using when_op<false, Tasks...>::await_ready;
    using when_op<false, Tasks...>::await_suspend;
    using when_op<false, Tasks...>::await_resume;
};

template <>
class when_any<> {
public:
    when_any() = delete;
};

template <detail::awaitable... Tasks>
when_all(Tasks...) -> when_all<detail::normalized_task_t<Tasks>...>;

template <detail::async_range Range>
when_all(Range) -> when_all<detail::range_tasks<detail::normalized_range_task_t<Range>>>;

template <detail::awaitable... Tasks>
    requires (sizeof...(Tasks) > 0)
when_any(Tasks...) -> when_any<detail::normalized_task_t<Tasks>...>;

template <detail::async_range Range>
when_any(Range) -> when_any<detail::range_tasks<detail::normalized_range_task_t<Range>>>;

}  // namespace kota
