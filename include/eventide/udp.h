#pragma once

#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "handle.h"
#include "task.h"

namespace eventide {

class event_loop;

template <typename Tag>
struct awaiter;

class udp : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    udp(udp&& other) noexcept;

    udp& operator=(udp&& other) noexcept;

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

    static result<udp> create(event_loop& loop);

    static result<udp> create(event_loop& loop, unsigned int flags);

    static result<udp> open(event_loop& loop, int fd);

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
    async_node* waiter = nullptr;
    result<recv_result>* active = nullptr;
    std::deque<result<recv_result>> pending;
    std::vector<char> buffer;
    bool receiving = false;

    async_node* send_waiter = nullptr;
    error* send_active = nullptr;
    std::optional<error> send_pending;
    bool send_inflight = false;
};

}  // namespace eventide
