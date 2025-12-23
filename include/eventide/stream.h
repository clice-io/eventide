#pragma once

#include <coroutine>
#include <cstddef>
#include <span>
#include <vector>
#include <string>

#include "handle.h"
#include "ringbuffer.h"
#include "task.h"

namespace eventide {

template <typename Derived>
class stream : public handle<Derived> {
public:
    task<std::string> read();

    task<> write(std::span<const char> data);

protected:
    stream() = default;

public:
    ring_buffer buffer;
    promise_base* reader;
};

class pipe : public stream<pipe> {
public:
    pipe(event_loop& loop, int fd);
};

class tcp_socket : public stream<tcp_socket> {};

class console : public stream<console> {};

}  // namespace eventide
