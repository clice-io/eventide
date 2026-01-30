#pragma once

#include <optional>
#include <string>

#include "error.h"
#include "handle.h"
#include "task.h"

namespace eventide {

class event_loop;

template <typename Tag>
struct awaiter;

class fs_event : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    struct change {
        std::string path;
        int flags;
    };

    static result<fs_event> create(event_loop& loop);

    /// Start watching the given path; flags passed directly to libuv.
    error start(const char* path, unsigned int flags = 0);

    error stop();

    /// Await a change event; delivers one pending change at a time.
    task<result<change>> wait();

private:
    async_node* waiter = nullptr;
    result<change>* active = nullptr;
    std::optional<change> pending;
};

}  // namespace eventide
