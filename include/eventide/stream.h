#pragma once

#include <coroutine>
#include <cstddef>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "ringbuffer.h"
#include "task.h"

namespace eventide {

class event_loop;

template <typename StreamT>
class acceptor;

class stream {
public:
    stream() noexcept;

    stream(const stream&) = delete;
    stream& operator=(const stream&) = delete;

    stream(stream&& other) noexcept;
    stream& operator=(stream&& other) noexcept;

    ~stream();

    struct Self;
    Self* operator->() noexcept;
    const Self* operator->() const noexcept;

    task<std::string> read();

    task<> write(std::span<const char> data);

protected:
    explicit stream(Self* state) noexcept;

    std::unique_ptr<Self, void (*)(void*)> self;
};

template <typename Stream>
class acceptor {
public:
    acceptor() noexcept;

    acceptor(const acceptor&) = delete;
    acceptor& operator=(const acceptor&) = delete;

    acceptor(acceptor&& other) noexcept;

    acceptor& operator=(acceptor&& other) noexcept;

    ~acceptor();

    struct Self;
    Self* operator->() noexcept;
    const Self* operator->() const noexcept;

    task<result<Stream>> accept();

private:
    explicit acceptor(Self* state) noexcept;

    std::unique_ptr<Self, void (*)(void*)> self;

    friend class pipe;
    friend class tcp_socket;
};

class pipe : public stream {
public:
    pipe() noexcept = default;

    using acceptor = eventide::acceptor<pipe>;

    static result<pipe> open(event_loop& loop, int fd);

    static result<acceptor> listen(event_loop& loop, const char* name, int backlog = 128);

    explicit pipe(Self* state) noexcept;

private:
    static result<pipe> create(event_loop& loop);

    void* native_handle() noexcept;
    const void* native_handle() const noexcept;

    friend class process;
};

class tcp_socket : public stream {
public:
    tcp_socket() noexcept = default;

    using acceptor = eventide::acceptor<tcp_socket>;

    static result<tcp_socket> open(event_loop& loop, int fd);

    static result<acceptor> listen(event_loop& loop,
                                   std::string_view host,
                                   int port,
                                   unsigned int flags = 0,
                                   int backlog = 128);

    explicit tcp_socket(Self* state) noexcept;

private:
};

class console : public stream {
public:
    console() noexcept = default;

    static result<console> open(event_loop& loop, int fd);

private:
    explicit console(Self* state) noexcept;
};

}  // namespace eventide
