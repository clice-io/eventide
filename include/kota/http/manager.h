#pragma once

#include <cstddef>
#include <unordered_map>

#include "kota/http/curl.h"
#include "kota/async/io/loop.h"

struct uv_handle_s;
using uv_handle_t = uv_handle_s;

struct uv_timer_s;
using uv_timer_t = uv_timer_s;

struct uv_poll_s;
using uv_poll_t = uv_poll_s;

namespace kota::http {

class manager {
public:
    manager(const manager&) = delete;
    manager& operator=(const manager&) = delete;

    manager(manager&&) = delete;
    manager& operator=(manager&&) = delete;

    ~manager();

    static manager& for_loop(event_loop& loop);

    static void unregister_loop(event_loop& loop);

    curl::multi_error add_request(CURL* easy) noexcept;

    curl::multi_error remove_request(CURL* easy) noexcept;

    void drive_timeout() noexcept;

    void drain_completed() noexcept;

    void drive_timeout_arming(void* arming_request) noexcept;

    void drain_completed_arming(void* arming_request) noexcept;

    std::size_t pending_requests() const noexcept {
        return active_requests;
    }

    event_loop& loop() const noexcept {
        return *bound_loop;
    }

    CURLM* native_multi() const noexcept {
        return multi.get();
    }

private:
    struct timer_context;
    struct socket_context;

    static int on_curl_socket(CURL* easy,
                              curl_socket_t socket,
                              int action,
                              void* userp,
                              void* socketp) noexcept;

    static int on_curl_timeout(CURLM* multi, long timeout_ms, void* userp) noexcept;

    static void on_uv_socket(uv_poll_t* handle, int status, int events) noexcept;

    static void on_uv_timeout(uv_timer_t* handle) noexcept;

    static void on_uv_socket_close(uv_handle_t* handle) noexcept;

    static void on_uv_timer_close(uv_handle_t* handle) noexcept;

    socket_context* ensure_socket(curl_socket_t socket) noexcept;

    void update_socket(curl_socket_t socket, int action, void* socketp) noexcept;

    void update_timeout(long timeout_ms) noexcept;

    void drive_socket(curl_socket_t socket, int flags) noexcept;

    void drain_completed_impl(void* arming_request) noexcept;

    void close_watchers() noexcept;

    manager(event_loop& loop, curl::multi_handle multi) noexcept;

    event_loop* bound_loop = nullptr;
    curl::multi_handle multi;
    timer_context* timer = nullptr;
    std::unordered_map<curl_socket_t, socket_context*> sockets;
    std::size_t active_requests = 0;
};

}  // namespace kota::http
