#pragma once

#include <deque>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "handle.h"
#include "async/task.h"

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

    static std::expected<udp, std::error_code> create(event_loop& loop);

    static std::expected<udp, std::error_code> create(event_loop& loop, unsigned int flags);

    static std::expected<udp, std::error_code> open(event_loop& loop, int fd);

    std::error_code bind(std::string_view host, int port, unsigned flags = 0);

    std::error_code connect(std::string_view host, int port);

    std::error_code disconnect();

    task<std::error_code> send(std::span<const char> data, std::string_view host, int port);

    task<std::error_code> send(std::span<const char> data);

    std::error_code try_send(std::span<const char> data, std::string_view host, int port);

    std::error_code try_send(std::span<const char> data);

    std::expected<endpoint, std::error_code> getsockname() const;

    std::expected<endpoint, std::error_code> getpeername() const;

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

    task<std::expected<recv_result, std::error_code>> recv();

private:
    async_frame* waiter = nullptr;
    std::expected<recv_result, std::error_code>* active = nullptr;
    std::deque<std::expected<recv_result, std::error_code>> pending;
    std::vector<char> buffer;
    bool receiving = false;

    async_frame* send_waiter = nullptr;
    std::error_code* send_active = nullptr;
    std::optional<std::error_code> send_pending;
    bool send_inflight = false;
};

}  // namespace eventide
