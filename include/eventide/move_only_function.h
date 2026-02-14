#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace eventide {

template <typename>
class move_only_function;

template <typename R, typename... Args>
class move_only_function<R(Args...)> {
    struct base {
        virtual ~base() = default;
        virtual R invoke(Args... args) = 0;
    };

    template <typename F>
    struct holder final : base {
        F fn;

        explicit holder(F&& f) : fn(std::forward<F>(f)) {}

        R invoke(Args... args) override {
            return std::invoke(fn, std::forward<Args>(args)...);
        }
    };

public:
    move_only_function() noexcept = default;

    move_only_function(std::nullptr_t) noexcept {}

    move_only_function(move_only_function&&) noexcept = default;
    move_only_function& operator=(move_only_function&&) noexcept = default;

    move_only_function(const move_only_function&) = delete;
    move_only_function& operator=(const move_only_function&) = delete;

    template <typename F,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, move_only_function>>>
    move_only_function(F&& f) {
        assign(std::forward<F>(f));
    }

    template <typename F,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, move_only_function>>>
    move_only_function& operator=(F&& f) {
        assign(std::forward<F>(f));
        return *this;
    }

    move_only_function& operator=(std::nullptr_t) noexcept {
        target.reset();
        return *this;
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(target);
    }

    R operator()(Args... args) {
        assert(target && "move_only_function invoked without a target");
        return target->invoke(std::forward<Args>(args)...);
    }

    void reset() noexcept {
        target.reset();
    }

private:
    template <typename F>
    void assign(F&& f) {
        using Fn = std::decay_t<F>;
        target = std::make_unique<holder<Fn>>(std::forward<F>(f));
    }

    std::unique_ptr<base> target{};
};

}  // namespace eventide
