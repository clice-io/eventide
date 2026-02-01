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

    struct watch_options {
        /// Report creation/removal events (if supported by backend).
        bool watch_entry;

        /// Use stat polling where available.
        bool stat;

        /// Recurse into subdirectories when supported.
        bool recursive;

        constexpr watch_options(bool watch_entry = false,
                                bool stat = false,
                                bool recursive = false) :
            watch_entry(watch_entry), stat(stat), recursive(recursive) {}
    };

    struct change_flags {
        /// Entry renamed or moved.
        bool rename;

        /// Entry content/metadata changed.
        bool change;

        constexpr change_flags(bool rename = false, bool change = false) :
            rename(rename), change(change) {}
    };

    struct change {
        std::string path;
        change_flags flags = {};
    };

    static result<fs_event> create(event_loop& loop = event_loop::current());

    /// Start watching the given path; flags mapped to libuv equivalents.
    error start(const char* path, watch_options options = watch_options{});

    error stop();

    /// Await a change event; delivers one pending change at a time.
    task<result<change>> wait();

private:
    explicit fs_event(Self* state) noexcept;

    std::unique_ptr<Self, void (*)(void*)> self;
};

}  // namespace eventide
