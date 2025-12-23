#pragma once

#include <coroutine>
#include <cstddef>
#include <span>
#include <vector>

#include "handle.h"
#include "ringbuffer.h"

namespace eventide {

template <typename Derived>
class stream : public handle<Derived> {
public:
    void read(std::span<char> buf);

    void write(std::span<const char> data);

protected:
    stream() = default;

private:
    ring_buffer buffer;
};

class pipe : public stream<pipe> {};

class tcp_socket : public stream<tcp_socket> {};

class console : public stream<console> {};

}  // namespace eventide
