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
protected:
    using handle::handle;

public:
    task<std::string> read();

    task<> write(std::span<const char> data);

public:
    /// a stream allows only one active reader at a time
    promise_base* reader;

    ring_buffer buffer;
};

class pipe : public stream {
private:
    using stream::stream;

public:
    static std::expected<pipe, std::error_code> open(event_loop& loop, int fd);
};

class tcp_socket : public stream {};

class console : public stream {};

}  // namespace eventide
