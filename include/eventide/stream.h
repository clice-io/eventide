#pragma once

#include <coroutine>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "error.h"
#include "handle.h"
#include "ringbuffer.h"
#include "task.h"

namespace eventide {

class stream : public handle {
public:
    task<std::string> read();

    task<> write(std::span<const char> data);

protected:
    using handle::handle;

public:
    ring_buffer buffer;
    promise_base* reader;
};

class pipe : public stream {
public:
    pipe(event_loop& loop, int fd);

    static std::expected<pipe, std::error_code> open(int fd);
};

class tcp_socket : public stream {};

class console : public stream {};

}  // namespace eventide
