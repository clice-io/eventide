#pragma once

#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
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

    std::error_code bind(std::string_view host, int port, unsigned flags = 0);

    std::error_code connect(std::string_view host, int port);

    std::error_code disconnect();

    task<std::error_code> send(std::span<const char> data, std::string_view host, int port);

    task<std::error_code> send(std::span<const char> data);

    std::error_code try_send(std::span<const char> data, std::string_view host, int port);

    std::error_code try_send(std::span<const char> data);

    result<endpoint> getsockname() const;

    result<endpoint> getpeername() const;

    std::error_code set_membership(std::string_view multicast_addr,
                                   std::string_view interface_addr,
                                   membership m);

    std::error_code set_source_membership(std::string_view multicast_addr,
                                          std::string_view interface_addr,
                                          std::string_view source_addr,
                                          membership m);

    std::error_code set_multicast_loop(bool on);

    std::error_code set_multicast_ttl(int ttl);

    std::error_code set_multicast_interface(std::string_view interface_addr);

    std::error_code set_broadcast(bool on);

    std::error_code set_ttl(int ttl);

    bool using_recvmmsg() const;

    std::size_t send_queue_size() const;

    std::size_t send_queue_count() const;

    std::error_code stop_recv();

    task<result<recv_result>> recv();

private:
    async_node* waiter = nullptr;
    result<recv_result>* active = nullptr;
    std::deque<result<recv_result>> pending;
    std::vector<char> buffer;
    bool receiving = false;

    async_node* send_waiter = nullptr;
    std::error_code* send_active = nullptr;
    std::optional<std::error_code> send_pending;
    bool send_inflight = false;
};

}  // namespace eventide
