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

    struct change {
        std::string path;
        int flags;
    };

    static result<fs_event> create(event_loop& loop = event_loop::current());

    /// Start watching the given path; flags passed directly to libuv.
    error start(const char* path, unsigned int flags = 0);

    error stop();

    /// Await a change event; delivers one pending change at a time.
    task<result<change>> wait();

private:
    explicit fs_event(Self* state) noexcept;

    std::unique_ptr<Self, void (*)(void*)> self;
};

}  // namespace eventide
