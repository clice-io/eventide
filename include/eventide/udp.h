#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "task.h"

namespace eventide {

class event_loop;

class udp {
public:
    udp() noexcept;

    udp(const udp&) = delete;
    udp& operator=(const udp&) = delete;

    udp(udp&& other) noexcept;

    udp& operator=(udp&& other) noexcept;

    ~udp();

    struct Self;
    Self* operator->() noexcept;
    const Self* operator->() const noexcept;

    enum class recv_flags : unsigned int {
        none = 0,
        partial = 1 << 0,
        mmsg_chunk = 1 << 1,
    };

    struct recv_result {
        std::string data;
        std::string addr;
        int port = 0;
        recv_flags flags = recv_flags::none;
    };

    struct endpoint {
        std::string addr;
        int port = 0;
    };

    enum class membership { join, leave };

    static result<udp> create(event_loop& loop = event_loop::current());

    enum class create_flags : unsigned int {
        none = 0,
        ipv6_only = 1 << 0,
        recvmmsg = 1 << 1,
    };

    enum class bind_flags : unsigned int {
        none = 0,
        ipv6_only = 1 << 0,
        reuse_addr = 1 << 1,
        reuse_port = 1 << 2,
    };

    static result<udp> create(create_flags flags, event_loop& loop = event_loop::current());

    static result<udp> open(int fd, event_loop& loop = event_loop::current());

    error bind(std::string_view host, int port, bind_flags flags = bind_flags::none);

    error connect(std::string_view host, int port);

    error disconnect();

    task<error> send(std::span<const char> data, std::string_view host, int port);

    task<error> send(std::span<const char> data);

    error try_send(std::span<const char> data, std::string_view host, int port);

    error try_send(std::span<const char> data);

    result<endpoint> getsockname() const;

    result<endpoint> getpeername() const;

    error set_membership(std::string_view multicast_addr,
                         std::string_view interface_addr,
                         membership m);

    error set_source_membership(std::string_view multicast_addr,
                                std::string_view interface_addr,
                                std::string_view source_addr,
                                membership m);

    error set_multicast_loop(bool on);

    error set_multicast_ttl(int ttl);

    error set_multicast_interface(std::string_view interface_addr);

    error set_broadcast(bool on);

    error set_ttl(int ttl);

    bool using_recvmmsg() const;

    std::size_t send_queue_size() const;

    std::size_t send_queue_count() const;

    error stop_recv();

    task<result<recv_result>> recv();

private:
    explicit udp(Self* state) noexcept;

    std::unique_ptr<Self, void (*)(void*)> self;
};

constexpr udp::create_flags operator|(udp::create_flags lhs, udp::create_flags rhs) noexcept {
    return static_cast<udp::create_flags>(static_cast<unsigned int>(lhs) |
                                          static_cast<unsigned int>(rhs));
}

constexpr udp::create_flags operator&(udp::create_flags lhs, udp::create_flags rhs) noexcept {
    return static_cast<udp::create_flags>(static_cast<unsigned int>(lhs) &
                                          static_cast<unsigned int>(rhs));
}

constexpr udp::create_flags& operator|=(udp::create_flags& lhs, udp::create_flags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool has_flag(udp::create_flags value, udp::create_flags flag) noexcept {
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0U;
}

constexpr udp::bind_flags operator|(udp::bind_flags lhs, udp::bind_flags rhs) noexcept {
    return static_cast<udp::bind_flags>(static_cast<unsigned int>(lhs) |
                                        static_cast<unsigned int>(rhs));
}

constexpr udp::bind_flags operator&(udp::bind_flags lhs, udp::bind_flags rhs) noexcept {
    return static_cast<udp::bind_flags>(static_cast<unsigned int>(lhs) &
                                        static_cast<unsigned int>(rhs));
}

constexpr udp::bind_flags& operator|=(udp::bind_flags& lhs, udp::bind_flags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool has_flag(udp::bind_flags value, udp::bind_flags flag) noexcept {
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0U;
}

constexpr udp::recv_flags operator|(udp::recv_flags lhs, udp::recv_flags rhs) noexcept {
    return static_cast<udp::recv_flags>(static_cast<unsigned int>(lhs) |
                                        static_cast<unsigned int>(rhs));
}

constexpr udp::recv_flags operator&(udp::recv_flags lhs, udp::recv_flags rhs) noexcept {
    return static_cast<udp::recv_flags>(static_cast<unsigned int>(lhs) &
                                        static_cast<unsigned int>(rhs));
}

constexpr udp::recv_flags& operator|=(udp::recv_flags& lhs, udp::recv_flags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool has_flag(udp::recv_flags value, udp::recv_flags flag) noexcept {
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0U;
}

}  // namespace eventide
