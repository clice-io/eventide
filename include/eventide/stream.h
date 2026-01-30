#pragma once

#include <coroutine>
#include <cstddef>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "handle.h"
#include "ringbuffer.h"
#include "task.h"

namespace eventide {

class event_loop;

template <typename Tag>
struct awaiter;

template <typename StreamT>
class acceptor;

class stream : public handle {
protected:
    using handle::handle;

    stream() = default;

    template <typename Tag>
    friend struct awaiter;

public:
    task<std::string> read();

    task<> write(std::span<const char> data);

private:
    /// a stream allows only one active reader at a time
    async_node* reader;

    ring_buffer buffer;
};

template <typename Stream>
class acceptor : public handle {
private:
    using handle::handle;

    friend Stream;

    template <typename Tag>
    friend struct awaiter;

public:
    acceptor(acceptor&& other) noexcept;

    acceptor& operator=(acceptor&& other) noexcept;

    task<result<Stream>> accept();

private:
    async_node* waiter = nullptr;
    result<Stream>* active = nullptr;
    std::deque<result<Stream>> pending;
};

class pipe : public stream {
private:
    using stream::stream;

    friend class process;

public:
    using acceptor = eventide::acceptor<pipe>;

    static result<pipe> open(event_loop& loop, int fd);

    static result<acceptor> listen(event_loop& loop, const char* name, int backlog = 128);
};

class tcp_socket : public stream {
private:
    using stream::stream;

public:
    using acceptor = eventide::acceptor<tcp_socket>;

    static result<tcp_socket> open(event_loop& loop, int fd);

    static result<acceptor> listen(event_loop& loop,
                                   std::string_view host,
                                   int port,
                                   unsigned int flags = 0,
                                   int backlog = 128);
};

class console : public stream {
private:
    using stream::stream;

public:
    static result<console> open(event_loop& loop, int fd);
};

}  // namespace eventide
