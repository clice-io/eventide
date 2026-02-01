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

    struct recv_result {
        std::string data;
        std::string addr;
        int port = 0;
        unsigned flags = 0;
    };

    struct endpoint {
        std::string addr;
        int port = 0;
    };

    enum class membership { join, leave };

    static result<udp> create(event_loop& loop = event_loop::current());

    static result<udp> create(unsigned int flags, event_loop& loop = event_loop::current());

    static result<udp> open(int fd, event_loop& loop = event_loop::current());

    error bind(std::string_view host, int port, unsigned flags = 0);

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

}  // namespace eventide
