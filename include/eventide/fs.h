#pragma once

#include <expected>
#include <optional>
#include <string>
#include <system_error>

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

    static std::expected<fs_event, std::error_code> create(event_loop& loop);

    /// Start watching the given path; flags passed directly to libuv.
    std::error_code start(const char* path, unsigned int flags = 0);

    std::error_code stop();

    /// Await a change event; delivers one pending change at a time.
    task<std::expected<change, std::error_code>> wait();

private:
    async_node* waiter = nullptr;
    std::expected<change, std::error_code>* active = nullptr;
    std::optional<change> pending;
};

}  // namespace eventide
