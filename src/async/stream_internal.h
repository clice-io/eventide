#pragma once

#include <deque>

#include "libuv.h"
#include "ringbuffer.h"
#include "eventide/async/stream.h"

namespace eventide {

struct stream_handle {
    union {
        uv_handle_t handle;
        uv_stream_t stream;
        uv_pipe_t pipe;
        uv_tcp_t tcp;
        uv_tty_t tty;
    };
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

struct stream::Self : uv_handle<stream::Self, uv_stream_t>, stream_handle {
    system_op* reader = nullptr;
    system_op* writer = nullptr;
    ring_buffer buffer{};
    error error_code{};
};

template <typename Stream>
struct acceptor<Stream>::Self : uv_handle<acceptor<Stream>::Self, uv_stream_t>, stream_handle {
    system_op* waiter = nullptr;
    result<Stream>* active = nullptr;
    std::deque<result<Stream>> pending;
    int pipe_ipc = 0;
};

}  // namespace eventide
