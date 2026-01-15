#pragma once

#include <cassert>
#include <coroutine>
#include <cstdint>
#include <source_location>

namespace eventide {

template <typename T, typename Tag, int Bits = 3>
class tagged_pointer {
private:
    constexpr static uintptr_t Mask = (1 << Bits) - 1;
    constexpr static uintptr_t PtrMask = ~Mask;

public:
    tagged_pointer() = default;

    tagged_pointer(T* ptr, Tag initial_tag = static_cast<Tag>(0)) {
        set(ptr, initial_tag);
    }

    T* ptr() const noexcept {
        return reinterpret_cast<T*>(data & PtrMask);
    }

    Tag tag() const noexcept {
        return static_cast<Tag>(data & Mask);
    }

    void set(T* ptr, Tag tag_val) noexcept {
        static_assert(alignof(T) >= (1 << Bits), "T alignment is insufficient for requested bits");
        static_assert(sizeof(Tag) * 8 >= Bits, "Tag type is too small");

        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t t = static_cast<uintptr_t>(tag_val);
        assert((p & Mask) == 0 && "Pointer not aligned");
        assert((t & PtrMask) == 0 && "Tag value too large for bits");

        data = p | t;
    }

    void set_tag(Tag tag_val) noexcept {
        uintptr_t t = static_cast<uintptr_t>(tag_val);
        assert((t & PtrMask) == 0 && "Tag value too large");
        data = (data & PtrMask) | t;
    }

    tagged_pointer& operator|=(Tag tag_mask) noexcept {
        data |= static_cast<uintptr_t>(tag_mask);
        return *this;
    }

    tagged_pointer& operator&=(Tag tag_mask) noexcept {
        uintptr_t current_tag = data & Mask;
        uintptr_t new_tag = current_tag & static_cast<uintptr_t>(tag_mask);
        data = (data & PtrMask) | new_tag;
        return *this;
    }

    friend tagged_pointer operator|(tagged_pointer lhs, Tag tag_mask) noexcept {
        lhs |= tag_mask;
        return lhs;
    }

    friend tagged_pointer operator&(tagged_pointer lhs, Tag tag_mask) noexcept {
        lhs &= tag_mask;
        return lhs;
    }

    T* operator->() const noexcept {
        return ptr();
    }

    T& operator*() const noexcept {
        return *ptr();
    }

    explicit operator bool() const noexcept {
        return ptr() != nullptr;
    }

private:
    uintptr_t data = 0;
};

struct async_frame {
    enum Policy : uint8_t {
        None = 0,
        ExplicitCancel = 1 << 0,   // bit 0
        InterceptCancel = 1 << 1,  // bit 1
    };

    enum State : uint8_t {
        Running = 0,
        Cancelled = 1 << 0,   // bit 0
        Disposable = 1 << 1,  // bit 1
        Finished = 1 << 2     // bit 2
    };

    /// Stores the raw address of the coroutine frame (handle).
    ///
    /// Theoretically, this is redundant because the promise object is embedded
    /// within the coroutine frame. However, deriving the frame address from `this`
    /// (via `from_promise`) requires knowing the concrete Promise type to account
    /// for the opaque compiler overhead (e.g., resume/destroy function pointers)
    /// located before the promise.
    ///
    /// Since this base class is type-erased, we cannot calculate that offset dynamically
    /// and must explicitly cache the handle address here (costing 1 pointer size).
    void* address = nullptr;

    /// Schedule-site captured for diagnostics (useful when walking async chains).
    std::source_location location = {};

    /// Currently awaited child coroutine frame (state bits tagged onto the pointer).
    tagged_pointer<async_frame, State> callee = nullptr;

    /// Awaiting parent coroutine frame (policy bits tagged onto the pointer).
    tagged_pointer<async_frame, Policy> caller = nullptr;

public:
    template <typename Callback>
    void on_cancel(Callback&& callback);

    std::coroutine_handle<> handle() {
        return std::coroutine_handle<>::from_address(address);
    }

    void set_state(State state) {
        callee |= state;
    }

    void set_policy(Policy policy) {
        caller |= policy;
    }

    bool is_explicit_cancel() {
        return caller.tag() & Policy::ExplicitCancel;
    }

    bool is_intercept_cancel() {
        return caller.tag() & Policy::InterceptCancel;
    }

    bool is_cancelled() {
        return callee.tag() & State::Cancelled;
    }

    bool is_disposable() {
        return callee.tag() & State::Disposable;
    }

    bool is_finished() {
        return callee.tag() & State::Finished;
    }

    using Self = async_frame;

    void destroy();

    // Cancel this frame and its callee chain by tagging each child as Cancelled.
    void cancel(this Self& self);

private:
    /// Determines the next coroutine to execute.
    ///
    /// If the task is in a normal state or cancellation is intercepted, returns
    /// the handle to resume. If the task is cancelled and not intercepted,
    /// this triggers a bottom-up chain destruction and returns noop.
    std::coroutine_handle<> continuation(this Self& self);

public:
    void resume() {
        return continuation().resume();
    }

    std::coroutine_handle<> suspend(this Self& self, async_frame& caller);

    std::coroutine_handle<> finish(this Self& self);

    void stacktrace(std::source_location location = std::source_location::current());

    static async_frame* current();
};

}  // namespace eventide
