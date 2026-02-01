#pragma once

#include <memory>
#include <optional>
#include <string>

#include "error.h"
#include "task.h"

namespace eventide {

class event_loop;

class fs_event {
public:
    fs_event() noexcept;

    fs_event(const fs_event&) = delete;
    fs_event& operator=(const fs_event&) = delete;

    fs_event(fs_event&& other) noexcept;
    fs_event& operator=(fs_event&& other) noexcept;

    ~fs_event();

    struct Self;
    Self* operator->() noexcept;
    const Self* operator->() const noexcept;

    enum class watch_flags : unsigned int {
        none = 0,
        watch_entry = 1 << 0,
        stat = 1 << 1,
        recursive = 1 << 2,
    };

    enum class change_flags : unsigned int {
        none = 0,
        rename = 1 << 0,
        change = 1 << 1,
    };

    struct change {
        std::string path;
        change_flags flags = change_flags::none;
    };

    static result<fs_event> create(event_loop& loop = event_loop::current());

    /// Start watching the given path; flags mapped to libuv equivalents.
    error start(const char* path, watch_flags flags = watch_flags::none);

    error stop();

    /// Await a change event; delivers one pending change at a time.
    task<result<change>> wait();

private:
    explicit fs_event(Self* state) noexcept;

    std::unique_ptr<Self, void (*)(void*)> self;
};

constexpr fs_event::watch_flags operator|(fs_event::watch_flags lhs,
                                          fs_event::watch_flags rhs) noexcept {
    return static_cast<fs_event::watch_flags>(static_cast<unsigned int>(lhs) |
                                              static_cast<unsigned int>(rhs));
}

constexpr fs_event::watch_flags operator&(fs_event::watch_flags lhs,
                                          fs_event::watch_flags rhs) noexcept {
    return static_cast<fs_event::watch_flags>(static_cast<unsigned int>(lhs) &
                                              static_cast<unsigned int>(rhs));
}

constexpr fs_event::watch_flags& operator|=(fs_event::watch_flags& lhs,
                                            fs_event::watch_flags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool has_flag(fs_event::watch_flags value, fs_event::watch_flags flag) noexcept {
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0U;
}

constexpr fs_event::change_flags operator|(fs_event::change_flags lhs,
                                           fs_event::change_flags rhs) noexcept {
    return static_cast<fs_event::change_flags>(static_cast<unsigned int>(lhs) |
                                               static_cast<unsigned int>(rhs));
}

constexpr fs_event::change_flags operator&(fs_event::change_flags lhs,
                                           fs_event::change_flags rhs) noexcept {
    return static_cast<fs_event::change_flags>(static_cast<unsigned int>(lhs) &
                                               static_cast<unsigned int>(rhs));
}

constexpr fs_event::change_flags& operator|=(fs_event::change_flags& lhs,
                                             fs_event::change_flags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool has_flag(fs_event::change_flags value, fs_event::change_flags flag) noexcept {
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0U;
}

}  // namespace eventide
