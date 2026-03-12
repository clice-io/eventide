#pragma once

#include <cassert>
#include <concepts>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace eventide {

template <typename T, typename E = void, typename C = void>
class outcome;

/// Concept: error types that participate in structured concurrency.
/// The framework calls should_propagate() to decide whether an error
/// cancels sibling tasks and propagates upward through aggregates.
template <typename E>
concept structured_error = std::movable<E> && requires(const E& e) {
    { e.should_propagate() } -> std::convertible_to<bool>;
};

/// Optional extension: error types with human-readable messages.
template <typename E>
concept descriptive_error = structured_error<E> && requires(const E& e) {
    { e.message() } -> std::convertible_to<std::string_view>;
};

/// Optional extension: error types that support retry semantics.
template <typename E>
concept retriable_error = structured_error<E> && requires(const E& e) {
    { e.is_retriable() } -> std::convertible_to<bool>;
};

// ============================================================================
// Box types: wrap values to disambiguate variant alternatives
// ============================================================================

namespace detail {

template <typename T>
struct ok_box {
    T value;
};

template <>
struct ok_box<void> {};

template <typename E>
struct err_box {
    E value;
};

template <typename C>
struct cancel_box {
    C value;
};

// Storage type selection via partial specialization

template <typename T, typename E, typename C>
struct outcome_storage {
    using type = std::variant<ok_box<T>, err_box<E>, cancel_box<C>>;
};

template <typename T, typename E>
struct outcome_storage<T, E, void> {
    using type = std::variant<ok_box<T>, err_box<E>>;
};

template <typename T, typename C>
struct outcome_storage<T, void, C> {
    using type = std::variant<ok_box<T>, cancel_box<C>>;
};

template <typename T>
struct outcome_storage<T, void, void> {
    using type = ok_box<T>;
};

template <typename T, typename E, typename C>
using outcome_storage_t = typename outcome_storage<T, E, C>::type;

}  // namespace detail

// ============================================================================
// Factory tag types for constructing outcomes
// ============================================================================

template <typename E>
struct outcome_error_t {
    E value;
};

template <typename E>
outcome_error_t<std::decay_t<E>> outcome_error(E&& e) {
    return {std::forward<E>(e)};
}

/// Tag for constructing void-value outcomes: co_return outcome_value();
struct outcome_ok_tag {};

inline outcome_ok_tag outcome_value() {
    return {};
}

// ============================================================================
// outcome_traits: compile-time introspection
// ============================================================================

template <typename T>
struct outcome_traits;

template <typename T, typename E, typename C>
struct outcome_traits<outcome<T, E, C>> {
    using value_type = T;
    using error_type = E;
    using cancel_type = C;
    constexpr static bool has_error_channel = !std::is_void_v<E>;
    constexpr static bool has_cancel_channel = !std::is_void_v<C>;
};

template <typename T>
constexpr bool is_outcome_v = false;

template <typename T, typename E, typename C>
constexpr bool is_outcome_v<outcome<T, E, C>> = true;

template <typename T>
constexpr bool has_cancel_channel_v = false;

template <typename T, typename E, typename C>
constexpr bool has_cancel_channel_v<outcome<T, E, C>> = !std::is_void_v<C>;

template <typename T>
constexpr bool has_error_channel_v = false;

template <typename T, typename E, typename C>
constexpr bool has_error_channel_v<outcome<T, E, C>> = !std::is_void_v<E>;

// ============================================================================
// outcome<T, E, C>: primary template (full three-state)
// ============================================================================

template <typename T, typename E, typename C>
class outcome {
    using storage_type = detail::outcome_storage_t<T, E, C>;
    storage_type storage;

public:
    // --- Value construction ---

    template <typename U = T>
        requires (!std::is_void_v<T>) && std::constructible_from<T, U&&> &&
                 (!is_outcome_v<std::decay_t<U>> || std::same_as<std::decay_t<U>, T>)
    outcome(U&& value) : storage(detail::ok_box<T>{T(std::forward<U>(value))}) {}

    outcome()
        requires std::is_void_v<T>
        : storage(detail::ok_box<void>{}) {}

    // --- Error construction ---

    template <typename U>
        requires (!std::is_void_v<E>) && std::constructible_from<E, U>
    outcome(outcome_error_t<U> e) : storage(detail::err_box<E>{E(std::move(e.value))}) {}

    // --- Cancel construction ---

    template <typename U>
        requires (!std::is_void_v<C>) && std::constructible_from<C, U>
    outcome(detail::cancel_box<U> c) : storage(detail::cancel_box<C>{C(std::move(c.value))}) {}

    // --- Void-value construction from tag ---

    outcome(outcome_ok_tag)
        requires std::is_void_v<T>
        : storage(detail::ok_box<void>{}) {}

    // --- State queries ---

    bool has_value() const noexcept {
        return std::holds_alternative<detail::ok_box<T>>(storage);
    }

    bool has_error() const noexcept
        requires (!std::is_void_v<E>)
    {
        return std::holds_alternative<detail::err_box<E>>(storage);
    }

    bool is_cancelled() const noexcept
        requires (!std::is_void_v<C>)
    {
        return std::holds_alternative<detail::cancel_box<C>>(storage);
    }

    explicit operator bool() const noexcept {
        return has_value();
    }

    // --- Value access ---

    auto& value() &
        requires (!std::is_void_v<T>)
    {
        assert(has_value());
        return std::get<detail::ok_box<T>>(storage).value;
    }

    const auto& value() const&
        requires (!std::is_void_v<T>)
    {
        assert(has_value());
        return std::get<detail::ok_box<T>>(storage).value;
    }

    auto&& value() &&
        requires(!std::is_void_v<T>) {
            assert(has_value());
            return std::move(std::get<detail::ok_box<T>>(storage).value);
        }

        auto& operator*() &
            requires (!std::is_void_v<T>)
    {
        return value();
    }

    const auto& operator*() const&
        requires (!std::is_void_v<T>)
    {
        return value();
    }

    auto&& operator*() && requires(!std::is_void_v<T>) { return std::move(*this).value(); }

                          auto* operator-> ()
                              requires (!std::is_void_v<T>)
    {
        return &value();
    }

    const auto* operator->() const
        requires (!std::is_void_v<T>)
    {
        return &value();
    }

    // --- Error access ---

    auto& error() &
        requires (!std::is_void_v<E>)
    {
        assert(has_error());
        return std::get<detail::err_box<E>>(storage).value;
    }

    const auto& error() const&
        requires (!std::is_void_v<E>)
    {
        assert(has_error());
        return std::get<detail::err_box<E>>(storage).value;
    }

    auto&& error() &&
        requires(!std::is_void_v<E>) {
            assert(has_error());
            return std::move(std::get<detail::err_box<E>>(storage).value);
        }

        // --- Cancel access ---

        auto& cancellation() &
            requires (!std::is_void_v<C>)
    {
        assert(is_cancelled());
        return std::get<detail::cancel_box<C>>(storage).value;
    }

    const auto& cancellation() const&
        requires (!std::is_void_v<C>)
    {
        assert(is_cancelled());
        return std::get<detail::cancel_box<C>>(storage).value;
    }

    auto&& cancellation() && requires(!std::is_void_v<C>) {
        assert(is_cancelled());
        return std::move(std::get<detail::cancel_box<C>>(storage).value);
    }
};

// ============================================================================
// outcome<T, void, void>: pure value specialization (no variant overhead)
// ============================================================================

template <typename T>
class outcome<T, void, void> {
    detail::ok_box<T> storage;

public:
    template <typename U = T>
        requires (!std::is_void_v<T>) && std::constructible_from<T, U&&> &&
                 (!is_outcome_v<std::decay_t<U>> || std::same_as<std::decay_t<U>, T>)
    outcome(U&& value) : storage{T(std::forward<U>(value))} {}

    outcome()
        requires std::is_void_v<T>
    {}

    outcome(outcome_ok_tag)
        requires std::is_void_v<T>
    {}

    constexpr bool has_value() const noexcept {
        return true;
    }

    constexpr explicit operator bool() const noexcept {
        return true;
    }

    auto& value() &
        requires (!std::is_void_v<T>)
    {
        return storage.value;
    }

    const auto& value() const&
        requires (!std::is_void_v<T>)
    {
        return storage.value;
    }

    auto&& value() && requires(!std::is_void_v<T>) { return std::move(storage.value); }

        auto& operator*() &
            requires (!std::is_void_v<T>)
    {
        return value();
    }

    const auto& operator*() const&
        requires (!std::is_void_v<T>)
    {
        return value();
    }

    auto&& operator*() && requires(!std::is_void_v<T>) { return std::move(*this).value(); }

                          auto* operator-> ()
                              requires (!std::is_void_v<T>)
    {
        return &value();
    }

    const auto* operator->() const
        requires (!std::is_void_v<T>)
    {
        return &value();
    }
};

}  // namespace eventide
