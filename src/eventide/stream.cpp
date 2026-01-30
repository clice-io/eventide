#include "eventide/stream.h"

#include <type_traits>
#include <utility>

#include "libuv.h"
#include "eventide/loop.h"

namespace eventide {

namespace {

struct stream_read_tag;

struct stream_write_tag;

struct pipe_acceptor_t;

struct tcp_acceptor_t;

}  // namespace

template <>
struct awaiter<stream_read_tag> {
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
                    auto reader = s->reader;
                    s->reader = nullptr;
                    reader->resume();
                }
            }
            return;
        }

        s->buffer.advance_write(static_cast<size_t>(nread));

        if(s->reader) {
            auto reader = s->reader;
            s->reader = nullptr;
            reader->resume();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> waiting) noexcept {
        target.reader = &waiting.promise();
        int err = uv_read_start(target.as<uv_stream_t>(), on_alloc, on_read);
        (void)err;
        return std::noop_coroutine();
    }

    void await_resume() noexcept {}
};

task<std::string> stream::read() {
    auto stream = as<uv_stream_t>();
    stream->data = this;

    if(buffer.readable_bytes() == 0) {
        co_await awaiter<stream_read_tag>{*this};
    }

    std::string out;
    out.resize(buffer.readable_bytes());
    buffer.read(out.data(), out.size());
    co_return out;
}

task<> stream::write(std::span<const char> data) {
    return task<>();
}

template <typename tag>
struct awaiter {
    constexpr inline static bool is_pipe_v = std::is_same_v<tag, pipe_acceptor_t>;
    using stream_t = std::conditional_t<is_pipe_v, pipe, tcp_socket>;
    using handle_type = std::conditional_t<is_pipe_v, uv_pipe_t, uv_tcp_t>;
    using promise_t = task<result<stream_t>>::promise_type;

    acceptor<stream_t>* self;
    result<stream_t> outcome = std::unexpected(error());

    static int init_stream(stream_t& stream, uv_loop_t* loop) {
        if constexpr(is_pipe_v) {
            return uv_pipe_init(loop, stream.template as<uv_pipe_t>(), 0);
        } else {
            return uv_tcp_init(loop, stream.template as<uv_tcp_t>());
        }
    }

    static void on_connection_cb(uv_stream_t* server, int status) {
        auto listener = static_cast<acceptor<stream_t>*>(server->data);
        if(listener == nullptr) {
            return;
        }

        on_connection(*listener, server, status);
    }

    static void on_connection(acceptor<stream_t>& listener, uv_stream_t* server, int status) {
        auto deliver = [&](result<stream_t>&& value) {
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
            deliver(std::unexpected(error(status)));
            return;
        }

        stream_t conn(sizeof(handle_type));
        auto uv_loop = server->loop;

        int err = init_stream(conn, uv_loop);
        if(err == 0) {
            conn.mark_initialized();
            err =
                uv_accept(server, reinterpret_cast<uv_stream_t*>(conn.template as<handle_type>()));
        }

        if(err != 0) {
            deliver(std::unexpected(error(err)));
        } else {
            deliver(std::move(conn));
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        self->active = &outcome;
        return std::noop_coroutine();
    }

    result<stream_t> await_resume() noexcept {
        self->waiter = nullptr;
        self->active = nullptr;
        return std::move(outcome);
    }
};

template <typename Stream>
acceptor<Stream>::acceptor(acceptor&& other) noexcept :
    handle(std::move(other)), waiter(other.waiter), active(other.active),
    pending(std::move(other.pending)) {
    other.waiter = nullptr;
    other.active = nullptr;

    if(initialized()) {
        if(auto* h = as<uv_handle_t>()) {
            h->data = this;
        }
    }
}

template <typename Stream>
acceptor<Stream>& acceptor<Stream>::operator=(acceptor&& other) noexcept {
    if(this == &other) {
        return *this;
    }

    handle::operator=(std::move(other));
    waiter = other.waiter;
    active = other.active;
    pending = std::move(other.pending);

    other.waiter = nullptr;
    other.active = nullptr;

    if(initialized()) {
        if(auto* h = as<uv_handle_t>()) {
            h->data = this;
        }
    }

    return *this;
}

template <typename Stream>
task<result<Stream>> acceptor<Stream>::accept() {
    if(!pending.empty()) {
        auto out = std::move(pending.front());
        pending.pop_front();
        co_return out;
    }

    if(waiter != nullptr) {
        co_return std::unexpected(error::connection_already_in_progress);
    }

    if constexpr(std::is_same_v<Stream, pipe>) {
        co_return co_await awaiter<pipe_acceptor_t>{this};
    } else {
        co_return co_await awaiter<tcp_acceptor_t>{this};
    }
}

template class acceptor<pipe>;
template class acceptor<tcp_socket>;

result<pipe> pipe::open(event_loop& loop, int fd) {
    auto h = pipe(sizeof(uv_pipe_t));

    auto handle = h.as<uv_pipe_t>();
    int errc = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, 0);
    if(errc != 0) {
        return std::unexpected(error(errc));
    }

    h.mark_initialized();

    errc = uv_pipe_open(handle, fd);
    if(errc != 0) {
        return std::unexpected(error(errc));
    }

    return h;
}

static int start_pipe_listen(acceptor<pipe>& acc, event_loop& loop, const char* name, int backlog) {
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
    err =
        uv_listen(reinterpret_cast<uv_stream_t*>(handle), backlog, awaiter<pipe>::on_connection_cb);
    return err;
}

result<pipe::acceptor> pipe::listen(event_loop& loop, const char* name, int backlog) {
    pipe::acceptor acc(sizeof(uv_pipe_t));
    int err = start_pipe_listen(acc, loop, name, backlog);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return acc;
}

result<tcp_socket> tcp_socket::open(event_loop& loop, int fd) {
    tcp_socket sock(sizeof(uv_tcp_t));
    auto handle = sock.as<uv_tcp_t>();

    int err = uv_tcp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    sock.mark_initialized();

    err = uv_tcp_open(handle, fd);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return sock;
}

static int start_tcp_listen(acceptor<tcp_socket>& acc,
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

    auto build_addr = [&](auto& out, auto fn) -> int {
        return fn(std::string(host).c_str(), port, &out);
    };

    ::sockaddr* addr_ptr = nullptr;

    // Try IPv6 first, fallback to IPv4.
    if(build_addr(*reinterpret_cast<sockaddr_in6*>(&storage), uv_ip6_addr) == 0) {
        addr_ptr = reinterpret_cast<sockaddr*>(&storage);
    } else if(build_addr(*reinterpret_cast<sockaddr_in*>(&storage), uv_ip4_addr) == 0) {
        addr_ptr = reinterpret_cast<sockaddr*>(&storage);
    } else {
        return error::invalid_argument.value();
    }

    err = uv_tcp_bind(handle, addr_ptr, flags);
    if(err != 0) {
        return err;
    }

    handle->data = &acc;
    err = uv_listen(reinterpret_cast<uv_stream_t*>(handle),
                    backlog,
                    awaiter<tcp_socket>::on_connection_cb);
    return err;
}

result<tcp_socket::acceptor> tcp_socket::listen(event_loop& loop,
                                                std::string_view host,
                                                int port,
                                                unsigned int flags,
                                                int backlog) {
    tcp_socket::acceptor acc(sizeof(uv_tcp_t));
    int err = start_tcp_listen(acc, loop, host, port, flags, backlog);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return acc;
}

result<console> console::open(event_loop& loop, int fd) {
    console con(sizeof(uv_tty_t));
    auto handle = con.as<uv_tty_t>();

    int err = uv_tty_init(static_cast<uv_loop_t*>(loop.handle()), handle, fd, 0);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    con.mark_initialized();
    return con;
}

}  // namespace eventide
