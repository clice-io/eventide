#pragma once

#include <memory>
#include <source_location>
#include <tuple>

#include "kota/support/functional.h"

struct uv_loop_s;
using uv_loop_t = uv_loop_s;

namespace kota {

class async_node;

template <typename T = void, typename E = void, typename C = void>
class task;

/// A thread-safe relay for posting callbacks to an event loop.
///
/// Creating a relay keeps the event loop alive until the relay is
/// destroyed.
///
/// A default-constructed or moved-from relay is inert: send() is a
/// safe no-op, and destruction has no effect.
///
/// Usage (one-shot):
///   auto relay = loop.create_relay();
///   some_system_async_api([relay = std::move(relay)](auto result) mutable {
///       relay.send([result] { /* runs on loop thread */ });
///   });
///
/// Usage (recurring):
///   relay notify = loop.create_relay();
///   // from any thread, repeatedly:
///   notify.send([&] { drain_buffer(); });
///   // destroy the relay (or let it go out of scope) to release the loop hold.
///
/// Ownership:
///   The relay object is single-owner and non-copyable. send() is
///   thread-safe with respect to other send() calls, but the relay
///   must not be destroyed while any send() call is in progress.
///
/// Lifetime:
///   The event_loop must outlive all relays created from it. Using a
///   relay after its event_loop is destroyed is undefined behavior.
///
/// Thread safety:
///   - Construction (create_relay) is NOT thread-safe; call it on the
///     loop thread before handing the relay off.
///   - send() is thread-safe and can be called multiple times.
///   - Destroying the relay releases the loop hold. Pending callbacks
///     that were already enqueued are still delivered, unless the
///     event_loop itself is being destroyed (which clears the queue).
class relay {
public:
    relay() noexcept = default;

    relay(const relay&) = delete;
    relay& operator=(const relay&) = delete;

    relay(relay&& other) noexcept;
    relay& operator=(relay&& other) noexcept;

    ~relay();

    /// Posts a callback to the event loop.
    ///
    /// Thread-safe. Can be called multiple times. Callbacks are executed
    /// on the loop thread in FIFO enqueue order. Concurrent producers
    /// are serialized by a mutex, so cross-thread ordering follows
    /// mutex acquisition order.
    void send(function<void()> callback);

    /// Opaque implementation detail. Defined in loop.cpp.
    struct Self;

private:
    friend class event_loop;

    explicit relay(Self* p) noexcept;

    Self* self = nullptr;
};

/// Runs an event loop backed by libuv.
///
/// All async operations (tasks, timers, I/O) require an event_loop.
/// Each thread may have at most one active loop (thread-local).
/// Use event_loop::current() inside a running loop to get a reference.
class event_loop {
public:
    event_loop();

    ~event_loop();

    /// Returns the event loop running on the current thread.
    static event_loop& current();

    /// Opaque implementation detail. Defined in loop.cpp.
    struct Self;

    /// Internal accessor for the implementation struct.
    Self* operator->() {
        return self.get();
    }

    friend class async_node;

public:
    operator uv_loop_t&() noexcept;

    operator const uv_loop_t&() const noexcept;

    int run();

    void stop();

    /// Creates a relay that keeps this event loop alive until destroyed.
    ///
    /// NOT thread-safe: must be called on the loop thread. The returned relay
    /// object can then be moved to another thread or captured in a system API
    /// callback, where relay::send() can be called thread-safely.
    relay create_relay();

    /// Registers a callback to run during event_loop destruction, before the
    /// underlying uv loop is closed.
    ///
    /// NOT thread-safe: intended for loop-affine subsystems that need to
    /// release handles tied to this loop.
    void on_destroy(function<void()> callback);

    /// Schedules a task for execution on this event loop.
    /// If the task is passed by rvalue (temporary), the loop takes ownership
    /// (sets root=true). The task will be destroyed after it completes.
    template <typename Task>
    void schedule(Task&& task, std::source_location location = std::source_location::current()) {
        auto& promise = task.h.promise();
        if constexpr(std::is_rvalue_reference_v<Task&&>) {
            promise.root = true;
            task.release();
        }

        schedule(static_cast<async_node&>(promise), location);
    }

private:
    void schedule(async_node& frame, std::source_location location);

    std::unique_ptr<Self> self;
};

/// Convenience: creates a loop, schedules all tasks, runs to completion,
/// and returns a tuple of their values (via task::value()).
template <typename... Tasks>
auto run(Tasks&&... tasks) {
    event_loop loop;
    (loop.schedule(tasks), ...);
    loop.run();
    return std::tuple(std::move(tasks.value())...);
}

}  // namespace kota
