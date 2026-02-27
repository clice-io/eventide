#pragma once

#include <array>
#include <cstddef>
#include <deque>

#include "libuv.h"
#include "ringbuffer.h"
#include "eventide/async/stream.h"

namespace eventide {

constexpr std::size_t max_size(std::size_t a, std::size_t b) {
    return a > b ? a : b;
}

constexpr std::size_t stream_handle_size =
    max_size(max_size(sizeof(uv_pipe_t), sizeof(uv_tcp_t)), sizeof(uv_tty_t));

constexpr std::size_t stream_handle_align =
    max_size(max_size(alignof(uv_pipe_t), alignof(uv_tcp_t)), alignof(uv_tty_t));

struct alignas(stream_handle_align) stream_handle_storage {
    std::array<std::byte, stream_handle_size> bytes{};
};

namespace stream_detail {

inline handle_type to_handle_type(uv_handle_type type) {
    switch(type) {
        case UV_FILE: return handle_type::file;
        case UV_TTY: return handle_type::tty;
        case UV_NAMED_PIPE: return handle_type::pipe;
        case UV_TCP: return handle_type::tcp;
        case UV_UDP: return handle_type::udp;
        default: return handle_type::unknown;
    }
}

}  // namespace stream_detail

struct stream::Self : uv_handle<stream::Self, stream_handle_storage> {
    stream_handle_storage handle{};
    system_op* reader = nullptr;
    system_op* writer = nullptr;
    ring_buffer buffer{};
    error error_code{};

    template <typename T>
    T& as() noexcept {
        return reinterpret_cast<T&>(handle);
    }

    // Keep libuv callbacks anchored to this state object before starting read operations.
    void bind_stream_userdata() noexcept {
        as<uv_stream_t>().data = this;
    }
};

template <typename Stream>
struct acceptor<Stream>::Self : uv_handle<acceptor<Stream>::Self, stream_handle_storage> {
    stream_handle_storage handle{};
    system_op* waiter = nullptr;
    result<Stream>* active = nullptr;
    std::deque<result<Stream>> pending;
    int pipe_ipc = 0;

    template <typename T>
    T& as() noexcept {
        return reinterpret_cast<T&>(handle);
    }
};

}  // namespace eventide
