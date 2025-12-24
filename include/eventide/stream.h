#pragma once

#include <coroutine>
#include <cstddef>
#include <deque>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "handle.h"
#include "ringbuffer.h"
#include "task.h"

namespace eventide {

template <typename Tag>
struct awaiter;

class stream : public handle {
protected:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    task<std::string> read();

    task<> write(std::span<const char> data);

private:
    /// a stream allows only one active reader at a time
    promise_base* reader;

    ring_buffer buffer;
};

class pipe : public stream {
private:
    using stream::stream;

public:
    class acceptor;

    static std::expected<pipe, std::error_code> open(event_loop& loop, int fd);
    static std::expected<acceptor, std::error_code> listen(event_loop& loop,
                                                           const char* name,
                                                           int backlog = 128);

    class acceptor : public handle {
    private:
        using handle::handle;

        friend class pipe;

        template <typename Tag>
        friend struct awaiter;

    public:
        task<std::expected<pipe, std::error_code>> accept();

    private:
        promise_base* waiter = nullptr;
        std::expected<pipe, std::error_code>* active = nullptr;
        std::deque<std::expected<pipe, std::error_code>> pending;
    };
};

class tcp_socket : public stream {
private:
    using stream::stream;

public:
    static std::expected<tcp_socket, std::error_code> open(event_loop& loop, int fd);

    class acceptor;

    static std::expected<acceptor, std::error_code> listen(event_loop& loop,
                                                           std::string_view host,
                                                           int port,
                                                           unsigned int flags = 0,
                                                           int backlog = 128);

    class acceptor : public handle {
    private:
        using handle::handle;
        friend class tcp_socket;

        template <typename Tag>
        friend struct awaiter;

    public:
        task<std::expected<tcp_socket, std::error_code>> accept();

    private:
        promise_base* waiter = nullptr;
        std::expected<tcp_socket, std::error_code>* active = nullptr;
        std::deque<std::expected<tcp_socket, std::error_code>> pending;
    };
};

class console : public stream {
private:
    using stream::stream;

public:
    static std::expected<console, std::error_code> open(event_loop& loop, int fd);
};

}  // namespace eventide
