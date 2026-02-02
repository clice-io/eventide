#include "eventide/stream.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "libuv.h"
#include "ringbuffer.h"
#include "eventide/loop.h"

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

struct stream::Self : uv_handle<stream::Self, stream_handle_storage> {
    stream_handle_storage handle{};
    system_op* reader = nullptr;
    system_op* writer = nullptr;
    ring_buffer buffer{};

    template <typename T>
    T* as() noexcept {
        return reinterpret_cast<T*>(&handle);
    }

    template <typename T>
    const T* as() const noexcept {
        return reinterpret_cast<const T*>(&handle);
    }

    void init_handle() noexcept {
        this->mark_initialized();
        auto* h = reinterpret_cast<uv_handle_t*>(&handle);
        h->data = this;
    }
};

template <typename Stream>
struct acceptor<Stream>::Self : uv_handle<acceptor<Stream>::Self, stream_handle_storage> {
    stream_handle_storage handle{};
    system_op* waiter = nullptr;
    result<Stream>* active = nullptr;
    std::deque<result<Stream>> pending;

    template <typename T>
    T* as() noexcept {
        return reinterpret_cast<T*>(&handle);
    }

    template <typename T>
    const T* as() const noexcept {
        return reinterpret_cast<const T*>(&handle);
    }

    void init_handle() noexcept {
        this->mark_initialized();
        auto* h = reinterpret_cast<uv_handle_t*>(&handle);
        h->data = this;
    }
};

namespace {

struct stream_read_await : system_op {
    stream::Self* self;

    explicit stream_read_await(stream::Self* state) : self(state) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<stream_read_await*>(op);
        if(aw->self) {
            auto* handle = aw->self->as<uv_stream_t>();
            if(handle) {
                uv_read_stop(handle);
            }
            aw->self->reader = nullptr;
        }
        aw->complete();
    }

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto s = static_cast<eventide::stream::Self*>(handle->data);
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

    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t*) {
        auto s = static_cast<eventide::stream::Self*>(stream->data);
        if(!s || nread <= 0) {
            if(s) {
                uv_read_stop(stream);
                if(s->reader) {
                    auto reader = s->reader;
                    s->reader = nullptr;
                    reader->complete();
                }
            }
            return;
        }

        s->buffer.advance_write(static_cast<size_t>(nread));

        if(s->reader) {
            auto reader = s->reader;
            s->reader = nullptr;
            reader->complete();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }

        self->reader = this;
        int err = uv_read_start(self->as<uv_stream_t>(), on_alloc, on_read);
        (void)err;
        return link_continuation(&waiting.promise(), location);
    }

    void await_resume() noexcept {}
};

struct stream_write_await : system_op {
    using promise_t = task<>::promise_type;

    stream::Self* self;
    std::vector<char> storage;
    uv_write_t req{};

    stream_write_await(stream::Self* state, std::span<const char> data) :
        self(state), storage(data.begin(), data.end()) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<stream_write_await*>(op);
        if(aw->self) {
            uv_cancel(reinterpret_cast<uv_req_t*>(&aw->req));
        }
    }

    static void on_write(uv_write_t* req, int) {
        auto* aw = static_cast<stream_write_await*>(req->data);
        if(!aw || !aw->self) {
            return;
        }

        if(aw->self->writer) {
            auto w = aw->self->writer;
            aw->self->writer = nullptr;
            w->complete();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }

        self->writer = this;
        req.data = this;

        uv_buf_t buf = uv_buf_init(storage.empty() ? nullptr : storage.data(),
                                   static_cast<unsigned>(storage.size()));
        int err = uv_write(&req, self->as<uv_stream_t>(), &buf, 1, on_write);
        if(err != 0) {
            self->writer = nullptr;
            return waiting;
        }

        return link_continuation(&waiting.promise(), location);
    }

    void await_resume() noexcept {
        if(self) {
            self->writer = nullptr;
        }
    }
};

struct pipe_accept_await : system_op {
    using promise_t = task<result<pipe>>::promise_type;

    acceptor<pipe>::Self* self;
    result<pipe> outcome = std::unexpected(error());

    explicit pipe_accept_await(acceptor<pipe>::Self* acceptor) : self(acceptor) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<pipe_accept_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
            aw->self->active = nullptr;
        }
        aw->complete();
    }

    static void on_connection_cb(uv_stream_t* server, int status) {
        auto listener = static_cast<acceptor<pipe>::Self*>(server->data);
        if(listener == nullptr) {
            return;
        }

        on_connection(*listener, server, status);
    }

    static void on_connection(acceptor<pipe>::Self& listener, uv_stream_t* server, int status) {
        auto deliver = [&](result<pipe>&& value) {
            detail::deliver_or_queue(listener.waiter,
                                     listener.active,
                                     listener.pending,
                                     std::move(value));
        };

        if(status < 0) {
            deliver(std::unexpected(error(status)));
            return;
        }

        std::unique_ptr<stream::Self, void (*)(void*)> state(new stream::Self(),
                                                             stream::Self::destroy);
        auto* handle = state->as<uv_pipe_t>();
        int err = uv_pipe_init(server->loop, handle, 0);
        if(err == 0) {
            state->init_handle();
            err = uv_accept(server, reinterpret_cast<uv_stream_t*>(handle));
        }

        if(err != 0) {
            deliver(std::unexpected(error(err)));
        } else {
            deliver(pipe(state.release()));
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = this;
        self->active = &outcome;
        return link_continuation(&waiting.promise(), location);
    }

    result<pipe> await_resume() noexcept {
        if(self) {
            self->waiter = nullptr;
            self->active = nullptr;
        }
        return std::move(outcome);
    }
};

struct tcp_accept_await : system_op {
    using promise_t = task<result<tcp_socket>>::promise_type;

    acceptor<tcp_socket>::Self* self;
    result<tcp_socket> outcome = std::unexpected(error());

    explicit tcp_accept_await(acceptor<tcp_socket>::Self* acceptor) : self(acceptor) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<tcp_accept_await*>(op);
        if(aw->self) {
            aw->self->waiter = nullptr;
            aw->self->active = nullptr;
        }
        aw->complete();
    }

    static void on_connection_cb(uv_stream_t* server, int status) {
        auto listener = static_cast<acceptor<tcp_socket>::Self*>(server->data);
        if(listener == nullptr) {
            return;
        }

        on_connection(*listener, server, status);
    }

    static void on_connection(acceptor<tcp_socket>::Self& listener,
                              uv_stream_t* server,
                              int status) {
        auto deliver = [&](result<tcp_socket>&& value) {
            detail::deliver_or_queue(listener.waiter,
                                     listener.active,
                                     listener.pending,
                                     std::move(value));
        };

        if(status < 0) {
            deliver(std::unexpected(error(status)));
            return;
        }

        std::unique_ptr<stream::Self, void (*)(void*)> state(new stream::Self(),
                                                             stream::Self::destroy);
        auto* handle = state->as<uv_tcp_t>();
        int err = uv_tcp_init(server->loop, handle);
        if(err == 0) {
            state->init_handle();
            err = uv_accept(server, reinterpret_cast<uv_stream_t*>(handle));
        }

        if(err != 0) {
            deliver(std::unexpected(error(err)));
        } else {
            deliver(tcp_socket(state.release()));
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }
        self->waiter = this;
        self->active = &outcome;
        return link_continuation(&waiting.promise(), location);
    }

    result<tcp_socket> await_resume() noexcept {
        if(self) {
            self->waiter = nullptr;
            self->active = nullptr;
        }
        return std::move(outcome);
    }
};

struct tcp_connect_await : system_op {
    using promise_t = task<result<tcp_socket>>::promise_type;

    std::unique_ptr<stream::Self, void (*)(void*)> state;
    uv_connect_t req{};
    sockaddr_storage addr{};
    result<tcp_socket> outcome = std::unexpected(error());
    bool ready = true;

    tcp_connect_await(std::unique_ptr<stream::Self, void (*)(void*)> state,
                      std::string_view host,
                      int port) : state(std::move(state)) {
        action = &on_cancel;

        auto resolved = detail::resolve_addr(host, port);
        if(!resolved.has_value()) {
            ready = false;
            outcome = std::unexpected(resolved.error());
            return;
        }

        addr = resolved->storage;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<tcp_connect_await*>(op);
        if(aw->state) {
            // uv_connect_t can't be cancelled; close the handle to trigger ECANCELED.
            aw->state.reset();
        }
    }

    static void on_connect(uv_connect_t* req, int status) {
        auto* aw = static_cast<tcp_connect_await*>(req->data);
        if(!aw) {
            return;
        }

        if(status < 0) {
            aw->outcome = std::unexpected(error(status));
        } else if(aw->state) {
            aw->outcome = tcp_socket(aw->state.release());
        } else {
            aw->outcome = std::unexpected(error::invalid_argument);
        }

        aw->complete();
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!state || !ready) {
            return waiting;
        }

        req.data = this;

        int err = uv_tcp_connect(&req,
                                 state->as<uv_tcp_t>(),
                                 reinterpret_cast<const sockaddr*>(&addr),
                                 on_connect);
        if(err != 0) {
            outcome = std::unexpected(error(err));
            return waiting;
        }

        return link_continuation(&waiting.promise(), location);
    }

    result<tcp_socket> await_resume() noexcept {
        return std::move(outcome);
    }
};

}  // namespace

stream::stream() noexcept : self(nullptr, nullptr) {}

stream::stream(stream&& other) noexcept = default;

stream& stream::operator=(stream&& other) noexcept = default;

stream::~stream() = default;

stream::Self* stream::operator->() noexcept {
    return self.get();
}

const stream::Self* stream::operator->() const noexcept {
    return self.get();
}

void* stream::handle() noexcept {
    return self ? &self->handle : nullptr;
}

const void* stream::handle() const noexcept {
    return self ? &self->handle : nullptr;
}

task<std::string> stream::read() {
    if(!self) {
        co_return std::string{};
    }

    auto stream_handle = self->as<uv_stream_t>();
    stream_handle->data = self.get();

    if(self->buffer.readable_bytes() == 0) {
        co_await stream_read_await{self.get()};
    }

    std::string out;
    out.resize(self->buffer.readable_bytes());
    self->buffer.read(out.data(), out.size());
    co_return out;
}

task<> stream::write(std::span<const char> data) {
    if(!self || !self->initialized() || data.empty()) {
        co_return;
    }

    if(self->writer != nullptr) {
        assert(false && "stream::write supports a single writer at a time");
        co_return;
    }

    co_await stream_write_await{self.get(), data};
}

stream::stream(Self* state) noexcept : self(state, Self::destroy) {}

template <typename Stream>
acceptor<Stream>::acceptor() noexcept : self(nullptr, nullptr) {}

template <typename Stream>
acceptor<Stream>::acceptor(acceptor&& other) noexcept = default;

template <typename Stream>
acceptor<Stream>& acceptor<Stream>::operator=(acceptor&& other) noexcept = default;

template <typename Stream>
acceptor<Stream>::~acceptor() = default;

template <typename Stream>
typename acceptor<Stream>::Self* acceptor<Stream>::operator->() noexcept {
    return self.get();
}

template <typename Stream>
const typename acceptor<Stream>::Self* acceptor<Stream>::operator->() const noexcept {
    return self.get();
}

template <typename Stream>
task<result<Stream>> acceptor<Stream>::accept() {
    if(!self) {
        co_return std::unexpected(error::invalid_argument);
    }

    if(!self->pending.empty()) {
        auto out = std::move(self->pending.front());
        self->pending.pop_front();
        co_return out;
    }

    if(self->waiter != nullptr) {
        co_return std::unexpected(error::connection_already_in_progress);
    }

    if constexpr(std::is_same_v<Stream, pipe>) {
        co_return co_await pipe_accept_await{self.get()};
    } else {
        co_return co_await tcp_accept_await{self.get()};
    }
}

template <typename Stream>
acceptor<Stream>::acceptor(Self* state) noexcept : self(state, Self::destroy) {}

template class acceptor<pipe>;
template class acceptor<tcp_socket>;

result<pipe> pipe::open(int fd, event_loop& loop) {
    auto pipe_res = create(loop);
    if(!pipe_res.has_value()) {
        return std::unexpected(pipe_res.error());
    }

    auto handle = static_cast<uv_pipe_t*>(pipe_res->handle());
    int errc = uv_pipe_open(handle, fd);
    if(errc != 0) {
        return std::unexpected(error(errc));
    }

    return std::move(*pipe_res);
}

static int start_pipe_listen(pipe::acceptor& acc, const char* name, int backlog, event_loop& loop) {
    auto* self = acc.operator->();
    if(!self) {
        return error::invalid_argument.value();
    }

    auto handle = self->template as<uv_pipe_t>();

    int err = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, 0);
    if(err != 0) {
        return err;
    }

    self->init_handle();

    err = uv_pipe_bind(handle, name);
    if(err != 0) {
        return err;
    }

    err = uv_listen(reinterpret_cast<uv_stream_t*>(handle),
                    backlog,
                    pipe_accept_await::on_connection_cb);
    return err;
}

result<pipe::acceptor> pipe::listen(const char* name, int backlog, event_loop& loop) {
    pipe::acceptor acc(new pipe::acceptor::Self());
    int err = start_pipe_listen(acc, name, backlog, loop);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return acc;
}

pipe::pipe(Self* state) noexcept : stream(state) {}

result<pipe> pipe::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto* handle = state->as<uv_pipe_t>();
    int errc = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, 0);
    if(errc != 0) {
        return std::unexpected(error(errc));
    }

    state->init_handle();
    return pipe(state.release());
}

tcp_socket::tcp_socket(Self* state) noexcept : stream(state) {}

result<tcp_socket> tcp_socket::open(int fd, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = state->as<uv_tcp_t>();

    int err = uv_tcp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->init_handle();

    err = uv_tcp_open(handle, fd);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return tcp_socket(state.release());
}

task<result<tcp_socket>> tcp_socket::connect(std::string_view host, int port, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = state->as<uv_tcp_t>();

    int err = uv_tcp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        co_return std::unexpected(error(err));
    }

    state->init_handle();

    co_return co_await tcp_connect_await{std::move(state), host, port};
}

static result<unsigned int> to_uv_tcp_bind_flags(const tcp_socket::bind_options& options) {
    unsigned int out = 0;
#ifdef UV_TCP_IPV6ONLY
    if(options.ipv6_only) {
        out |= UV_TCP_IPV6ONLY;
    }
#else
    if(options.ipv6_only) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

static int start_tcp_listen(tcp_socket::acceptor& acc,
                            std::string_view host,
                            int port,
                            tcp_socket::bind_options options,
                            int backlog,
                            event_loop& loop) {
    auto* self = acc.operator->();
    if(!self) {
        return error::invalid_argument.value();
    }

    auto handle = self->template as<uv_tcp_t>();

    int err = uv_tcp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return err;
    }

    self->init_handle();

    auto resolved = detail::resolve_addr(host, port);
    if(!resolved.has_value()) {
        return resolved.error().value();
    }

    ::sockaddr* addr_ptr = reinterpret_cast<sockaddr*>(&resolved->storage);

    auto uv_flags = to_uv_tcp_bind_flags(options);
    if(!uv_flags.has_value()) {
        return uv_flags.error().value();
    }

    err = uv_tcp_bind(handle, addr_ptr, uv_flags.value());
    if(err != 0) {
        return err;
    }

    err = uv_listen(reinterpret_cast<uv_stream_t*>(handle),
                    backlog,
                    tcp_accept_await::on_connection_cb);
    return err;
}

result<tcp_socket::acceptor> tcp_socket::listen(std::string_view host,
                                                int port,
                                                tcp_socket::bind_options options,
                                                int backlog,
                                                event_loop& loop) {
    tcp_socket::acceptor acc(new tcp_socket::acceptor::Self());
    int err = start_tcp_listen(acc, host, port, options, backlog, loop);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return acc;
}

result<console> console::open(int fd, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = state->as<uv_tty_t>();

    int err = uv_tty_init(static_cast<uv_loop_t*>(loop.handle()), handle, fd, 0);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->init_handle();
    return console(state.release());
}

console::console(Self* state) noexcept : stream(state) {}

}  // namespace eventide
