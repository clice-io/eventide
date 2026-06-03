#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "kota/support/config.h"
#include "kota/support/type_list.h"
#include "kota/support/type_traits.h"
#include "kota/async/runtime/node.h"
#include "kota/async/runtime/task.h"
#include "kota/async/vocab/awaitable.h"
#include "kota/async/vocab/outcome.h"

namespace kota::detail {

template <typename T>
constexpr inline bool is_task_v = is_specialization_of<task, std::remove_cvref_t<T>>;

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
    requires (!is_task_v<Awaitable>) && (!std::is_reference_v<Awaitable>) &&
             std::constructible_from<std::remove_cvref_t<Awaitable>, Awaitable&&> &&
             awaitable<std::remove_cvref_t<Awaitable>&&>
auto normalize_task_impl(std::remove_cvref_t<Awaitable> value)
    -> task<normalized_await_result_t<Awaitable>> {
    if constexpr(!std::is_void_v<normalized_await_result_t<Awaitable>>) {
        co_return co_await std::move(value);
    } else {
        co_await std::move(value);
    }
}

template <typename Awaitable>
    requires (!is_task_v<Awaitable>) && (!std::is_reference_v<Awaitable>) &&
             std::constructible_from<std::remove_cvref_t<Awaitable>, Awaitable&&> &&
             awaitable<std::remove_cvref_t<Awaitable>&&>
auto normalize_task(Awaitable&& input) -> task<normalized_await_result_t<Awaitable>> {
    return normalize_task_impl<Awaitable>(
        std::remove_cvref_t<Awaitable>(std::forward<Awaitable>(input)));
}

template <typename T, typename E, typename C>
async_node* node_from(task<T, E, C>& t) {
    return t.operator->();
}

template <typename Task>
using task_error_type_t = typename Task::error_type;

template <typename Task>
using task_cancel_type_t = typename Task::cancel_type;

template <typename T>
struct keep_non_void : std::bool_constant<!std::is_void_v<T>> {};

template <typename... Ts>
using aggregated_channel_t = typename type_list_to_union<
    type_list_unique_t<type_list_filter_t<type_list<Ts...>, keep_non_void>>>::type;

template <typename T>
using promote_void_cancel_t = std::conditional_t<std::is_void_v<T>, cancellation, T>;

template <typename... Ts>
constexpr inline bool any_non_void_v = (!std::is_void_v<Ts> || ...);

template <typename... Ts>
using aggregated_cancel_t = std::
    conditional_t<any_non_void_v<Ts...>, aggregated_channel_t<promote_void_cancel_t<Ts>...>, void>;

template <typename... Ts>
using task_group_error_type_t =
    typename type_list_to_union<type_list_unique_t<type_list<Ts...>>>::type;

template <typename Task>
using task_result_t = decltype(std::declval<Task&>().result());

template <typename Success, typename E, typename C>
using aggregate_result_t =
    std::conditional_t<std::is_void_v<E> && std::is_void_v<C>, Success, outcome<Success, E, C>>;

template <bool CaptureCancel, typename Result>
auto strip_channels_from_result(Result&& result) {
    return std::forward<Result>(result);
}

template <bool CaptureCancel, typename T, typename E, typename C>
auto strip_channels_from_result(outcome<T, E, C>&& result) {
    using type = std::conditional_t<std::is_void_v<C> || CaptureCancel,
                                    std::conditional_t<std::is_void_v<T>, std::nullopt_t, T>,
                                    outcome<T, void, C>>;

    if constexpr(!std::is_void_v<E>) {
        assert(!result.has_error());
    }

    if constexpr(!std::is_void_v<C>) {
        if constexpr(!CaptureCancel) {
            if(result.is_cancelled()) {
                return type(outcome_cancel(std::move(result).cancellation()));
            }
        } else {
            assert(!result.is_cancelled());
        }
    }

    if constexpr(std::is_void_v<T>) {
        if constexpr(std::is_void_v<C> || CaptureCancel) {
            return std::nullopt;
        } else {
            return type();
        }
    } else {
        if constexpr(std::is_void_v<C> || CaptureCancel) {
            return std::move(*result);
        } else {
            return type(std::move(*result));
        }
    }
}

template <typename Task, bool CaptureCancel>
using task_success_t =
    decltype(strip_channels_from_result<CaptureCancel>(std::declval<task_result_t<Task>>()));

template <typename Task>
auto take_result(Task& task) {
    return task.result();
}

template <bool CaptureCancel, typename Task>
auto take_success_result(Task& task) {
    return strip_channels_from_result<CaptureCancel>(take_result(task));
}

template <typename Task>
struct range_tasks {
    using task_type = Task;
};

template <typename Range>
using range_async_value_t = std::ranges::range_value_t<Range>;

template <typename Range>
using normalized_range_task_t = normalized_task_t<range_async_value_t<Range>>;

template <typename Range>
concept async_range = std::ranges::input_range<Range> && awaitable<range_async_value_t<Range>>;

template <typename Return, std::size_t I = 0, typename Tuple, typename F>
Return tuple_visit_at_return(std::size_t index, Tuple& tuple, F&& f) {
    if constexpr(I < std::tuple_size_v<std::remove_reference_t<Tuple>>) {
        if(index == I) {
            return f(std::integral_constant<std::size_t, I>{}, std::get<I>(tuple));
        }
        return tuple_visit_at_return<Return, I + 1>(index, tuple, std::forward<F>(f));
    } else {
        assert(false && "tuple_visit_at_return index out of bounds");
        std::abort();
    }
}

[[noreturn]] inline void fail_empty_when_any_range() {
#if KOTA_ENABLE_EXCEPTIONS
    throw std::invalid_argument("when_any(range) requires a non-empty range");
#else
    assert(false && "when_any(range) requires a non-empty range");
    KOTA_THROW(std::invalid_argument("when_any(range) requires a non-empty range"));
#endif
}

}  // namespace kota::detail
