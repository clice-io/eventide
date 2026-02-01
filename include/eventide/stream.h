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
#include "task.h"

namespace eventide {

class event_loop;

template <typename Stream>
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

    void* handle() noexcept;
    const void* handle() const noexcept;

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
    friend class pipe;
    friend class tcp_socket;

    explicit acceptor(Self* state) noexcept;

    std::unique_ptr<Self, void (*)(void*)> self;
};

class pipe : public stream {
public:
    pipe() noexcept = default;

    using acceptor = eventide::acceptor<pipe>;

    static result<pipe> open(int fd, event_loop& loop = event_loop::current());

    static result<acceptor> listen(const char* name,
                                   int backlog = 128,
                                   event_loop& loop = event_loop::current());

    explicit pipe(Self* state) noexcept;

private:
    friend class process;

    static result<pipe> create(event_loop& loop = event_loop::current());
};

class tcp_socket : public stream {
public:
    tcp_socket() noexcept = default;

    explicit tcp_socket(Self* state) noexcept;

    using acceptor = eventide::acceptor<tcp_socket>;

    enum class bind_flags : unsigned int {
        none = 0,
        ipv6_only = 1 << 0,
    };

    static result<tcp_socket> open(int fd, event_loop& loop = event_loop::current());

    static task<result<tcp_socket>> connect(std::string_view host,
                                            int port,
                                            event_loop& loop = event_loop::current());

    static result<acceptor> listen(std::string_view host,
                                   int port,
                                   bind_flags flags = bind_flags::none,
                                   int backlog = 128,
                                   event_loop& loop = event_loop::current());
};

class console : public stream {
public:
    console() noexcept = default;

    static result<console> open(int fd, event_loop& loop = event_loop::current());

private:
    explicit console(Self* state) noexcept;
};

constexpr tcp_socket::bind_flags operator|(tcp_socket::bind_flags lhs,
                                           tcp_socket::bind_flags rhs) noexcept {
    return static_cast<tcp_socket::bind_flags>(static_cast<unsigned int>(lhs) |
                                               static_cast<unsigned int>(rhs));
}

constexpr tcp_socket::bind_flags operator&(tcp_socket::bind_flags lhs,
                                           tcp_socket::bind_flags rhs) noexcept {
    return static_cast<tcp_socket::bind_flags>(static_cast<unsigned int>(lhs) &
                                               static_cast<unsigned int>(rhs));
}

constexpr tcp_socket::bind_flags& operator|=(tcp_socket::bind_flags& lhs,
                                             tcp_socket::bind_flags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool has_flag(tcp_socket::bind_flags value, tcp_socket::bind_flags flag) noexcept {
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0U;
}

}  // namespace eventide
