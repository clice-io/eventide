#include "eventide/stream.h"

#include <utility>

#include "libuv.h"
#include "eventide/loop.h"

namespace eventide {

namespace {

struct read_tag {};

struct accept_tag {};

struct tcp_accept_tag {};

}  // namespace

template <>
struct awaiter<read_tag> {
    stream& target;

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto s = static_cast<eventide::stream*>(handle->data);
        if(!s) {
            buf->base = nullptr;
            buf->len = 0;
            return;
        }

        auto [dst, writable] = s->buffer.get_write_ptr();
        buf->base = dst;
        buf->len = writable;

        if(writable == 0) {
            uv_read_stop(reinterpret_cast<uv_stream_t*>(handle));
        }
    }

    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
        auto s = static_cast<eventide::stream*>(stream->data);
        if(!s || nread <= 0) {
            if(s) {
                uv_read_stop(stream);
                if(s->reader) {
                    s->reader->resume();
                }
            }
            return;
        }

        s->buffer.advance_write(static_cast<size_t>(nread));

        if(s->reader) {
            s->reader->resume();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> waiting) noexcept {
        target.reader = &waiting.promise();
        int err = uv_read_start(target.as<uv_stream_t>(), on_alloc, on_read);
        return std::noop_coroutine();
    }

    void await_resume() noexcept {}
};

template <>
struct awaiter<accept_tag> {
    using promise_t = task<std::expected<pipe, std::error_code>>::promise_type;

    pipe::acceptor* self;
    std::expected<pipe, std::error_code> result = std::unexpected(std::error_code{});

    static int start_listen(pipe::acceptor& acc, event_loop& loop, const char* name, int backlog) {
        auto handle = acc.as<uv_pipe_t>();

        int err = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, 0);
        if(err != 0) {
            return err;
        }

        acc.mark_initialized();

        err = uv_pipe_bind(handle, name);
        if(err != 0) {
            return err;
        }

        handle->data = &acc;
        err = uv_listen(reinterpret_cast<uv_stream_t*>(handle), backlog, on_connection_cb);
        return err;
    }

    static void on_connection_cb(uv_stream_t* server, int status) {
        auto listener = static_cast<pipe::acceptor*>(server->data);
        if(listener == nullptr) {
            return;
        }

        on_connection(*listener, server, status);
    }

    static void on_connection(pipe::acceptor& listener, uv_stream_t* server, int status) {
        auto deliver = [&](std::expected<pipe, std::error_code>&& value) {
            if(listener.waiter && listener.active) {
                *listener.active = std::move(value);

                auto w = listener.waiter;
                listener.waiter = nullptr;
                listener.active = nullptr;

                w->resume();
            } else {
                listener.pending.push_back(std::move(value));
            }
        };

        if(status < 0) {
            deliver(std::unexpected(uv_error(status)));
            return;
        }

        pipe conn(sizeof(uv_pipe_t));
        auto uv_loop = server->loop;

        int err = uv_pipe_init(uv_loop, conn.as<uv_pipe_t>(), 0);
        if(err == 0) {
            conn.mark_initialized();
            err = uv_accept(server, reinterpret_cast<uv_stream_t*>(conn.as<uv_pipe_t>()));
        }

        if(err != 0) {
            deliver(std::unexpected(uv_error(err)));
        } else {
            deliver(std::move(conn));
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        self->active = &result;
        return std::noop_coroutine();
    }

    std::expected<pipe, std::error_code> await_resume() noexcept {
        self->waiter = nullptr;
        self->active = nullptr;
        return std::move(result);
    }
};

template <>
struct awaiter<tcp_accept_tag> {
    using promise_t = task<std::expected<tcp_socket, std::error_code>>::promise_type;

    tcp_socket::acceptor* self;
    std::expected<tcp_socket, std::error_code> result = std::unexpected(std::error_code{});

    static int start_listen(tcp_socket::acceptor& acc,
                            event_loop& loop,
                            std::string_view host,
                            int port,
                            unsigned int flags,
                            int backlog) {
        auto handle = acc.as<uv_tcp_t>();

        int err = uv_tcp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
        if(err != 0) {
            return err;
        }

        acc.mark_initialized();

        ::sockaddr_storage storage{};
        int addrlen = 0;

        auto build_addr = [&](auto& out, auto fn) -> int {
            addrlen = static_cast<int>(sizeof(out));
            return fn(std::string(host).c_str(), port, &out);
        };

        ::sockaddr* addr_ptr = nullptr;

        // Try IPv6 first, fallback to IPv4.
        if(build_addr(*reinterpret_cast<sockaddr_in6*>(&storage), uv_ip6_addr) == 0) {
            addr_ptr = reinterpret_cast<sockaddr*>(&storage);
        } else if(build_addr(*reinterpret_cast<sockaddr_in*>(&storage), uv_ip4_addr) == 0) {
            addr_ptr = reinterpret_cast<sockaddr*>(&storage);
        } else {
            return UV_EINVAL;
        }

        err = uv_tcp_bind(handle, addr_ptr, flags);
        if(err != 0) {
            return err;
        }

        handle->data = &acc;
        err = uv_listen(reinterpret_cast<uv_stream_t*>(handle), backlog, on_connection_cb);
        return err;
    }

    static void on_connection_cb(uv_stream_t* server, int status) {
        auto listener = static_cast<tcp_socket::acceptor*>(server->data);
        if(listener == nullptr) {
            return;
        }

        on_connection(*listener, server, status);
    }

    static void on_connection(tcp_socket::acceptor& listener, uv_stream_t* server, int status) {
        auto deliver = [&](std::expected<tcp_socket, std::error_code>&& value) {
            if(listener.waiter && listener.active) {
                *listener.active = std::move(value);

                auto w = listener.waiter;
                listener.waiter = nullptr;
                listener.active = nullptr;

                w->resume();
            } else {
                listener.pending.push_back(std::move(value));
            }
        };

        if(status < 0) {
            deliver(std::unexpected(uv_error(status)));
            return;
        }

        tcp_socket conn(sizeof(uv_tcp_t));
        auto uv_loop = server->loop;

        int err = uv_tcp_init(uv_loop, conn.as<uv_tcp_t>());
        if(err == 0) {
            conn.mark_initialized();
            err = uv_accept(server, reinterpret_cast<uv_stream_t*>(conn.as<uv_tcp_t>()));
        }

        if(err != 0) {
            deliver(std::unexpected(uv_error(err)));
        } else {
            deliver(std::move(conn));
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        self->active = &result;
        return std::noop_coroutine();
    }

    std::expected<tcp_socket, std::error_code> await_resume() noexcept {
        self->waiter = nullptr;
        self->active = nullptr;
        return std::move(result);
    }
};

task<std::string> stream::read() {
    auto stream = as<uv_stream_t>();
    stream->data = this;

    if(buffer.readable_bytes() == 0) {
        co_await awaiter<read_tag>{*this};
    }

    std::string out;
    out.resize(buffer.readable_bytes());
    buffer.read(out.data(), out.size());
    co_return out;
}

task<> stream::write(std::span<const char> data) {
    return task<>();
}

std::expected<pipe, std::error_code> pipe::open(event_loop& loop, int fd) {
    auto h = pipe(sizeof(uv_pipe_t));

    auto handle = h.as<uv_pipe_t>();
    int errc = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, 0);
    if(errc != 0) {
        return std::unexpected(uv_error(errc));
    }

    h.mark_initialized();

    errc = uv_pipe_open(handle, fd);
    if(errc != 0) {
        return std::unexpected(uv_error(errc));
    }

    return h;
}

std::expected<tcp_socket, std::error_code> tcp_socket::open(event_loop& loop, int fd) {
    tcp_socket sock(sizeof(uv_tcp_t));
    auto handle = sock.as<uv_tcp_t>();

    int err = uv_tcp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    sock.mark_initialized();

    err = uv_tcp_open(handle, fd);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    return sock;
}

std::expected<console, std::error_code> console::open(event_loop& loop, int fd) {
    console con(sizeof(uv_tty_t));
    auto handle = con.as<uv_tty_t>();

    int err = uv_tty_init(static_cast<uv_loop_t*>(loop.handle()), handle, fd, 0);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    con.mark_initialized();
    return con;
}

task<std::expected<pipe, std::error_code>> pipe::acceptor::accept() {
    if(!pending.empty()) {
        auto out = std::move(pending.front());
        pending.pop_front();
        co_return out;
    }

    if(waiter != nullptr) {
        co_return std::unexpected(uv_error(UV_EALREADY));
    }

    co_return co_await awaiter<accept_tag>{this};
}

task<std::expected<tcp_socket, std::error_code>> tcp_socket::acceptor::accept() {
    if(!pending.empty()) {
        auto out = std::move(pending.front());
        pending.pop_front();
        co_return out;
    }

    if(waiter != nullptr) {
        co_return std::unexpected(uv_error(UV_EALREADY));
    }

    co_return co_await awaiter<tcp_accept_tag>{this};
}

std::expected<pipe::acceptor, std::error_code> pipe::listen(event_loop& loop,
                                                            const char* name,
                                                            int backlog) {
    pipe::acceptor acc(sizeof(uv_pipe_t));
    int err = awaiter<accept_tag>::start_listen(acc, loop, name, backlog);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    return acc;
}

std::expected<tcp_socket::acceptor, std::error_code> tcp_socket::listen(event_loop& loop,
                                                                        std::string_view host,
                                                                        int port,
                                                                        unsigned int flags,
                                                                        int backlog) {
    tcp_socket::acceptor acc(sizeof(uv_tcp_t));
    int err = awaiter<tcp_accept_tag>::start_listen(acc, loop, host, port, flags, backlog);
    if(err != 0) {
        return std::unexpected(uv_error(err));
    }

    return acc;
}

}  // namespace eventide
