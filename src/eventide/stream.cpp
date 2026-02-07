#include "eventide/stream.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
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

namespace {

static handle_type to_handle_type(uv_handle_type type) {
    switch(type) {
        case UV_FILE: return handle_type::file;
        case UV_TTY: return handle_type::tty;
        case UV_NAMED_PIPE: return handle_type::pipe;
        case UV_TCP: return handle_type::tcp;
        case UV_UDP: return handle_type::udp;
        default: return handle_type::unknown;
    }
}

}  // namespace

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
    int pipe_ipc = 0;

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

static result<unsigned int> to_uv_pipe_flags(const pipe::options& opts);

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

    // When nread=0, it means no data was read but the stream is still alive (e.g., EAGAIN).
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t*) {
        auto s = static_cast<eventide::stream::Self*>(stream->data);
        if(!s || nread < 0) {
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

struct stream_read_some_await : system_op {
    using promise_t = task<std::size_t>::promise_type;

    stream::Self* self;
    std::span<char> dst;
    std::size_t bytes = 0;

    stream_read_some_await(stream::Self* state, std::span<char> buffer) : self(state), dst(buffer) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<stream_read_some_await*>(op);
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

        auto* aw = static_cast<stream_read_some_await*>(s->reader);
        if(!aw || aw->dst.empty()) {
            buf->base = nullptr;
            buf->len = 0;
            return;
        }

        buf->base = aw->dst.data();
        buf->len = static_cast<unsigned int>(aw->dst.size());
    }

    // When nread=0, it means no data was read but the stream is still alive (e.g., EAGAIN).
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t*) {
        auto s = static_cast<eventide::stream::Self*>(stream->data);
        if(!s) {
            return;
        }

        auto* aw = static_cast<stream_read_some_await*>(s->reader);
        if(!aw) {
            return;
        }

        if(nread < 0) {
            aw->bytes = 0;
        } else {
            aw->bytes = static_cast<std::size_t>(nread);
        }

        uv_read_stop(stream);
        s->reader = nullptr;
        aw->complete();
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
        if(err != 0) {
            self->reader = nullptr;
            return waiting;
        }

        return link_continuation(&waiting.promise(), location);
    }

    std::size_t await_resume() noexcept {
        if(self) {
            self->reader = nullptr;
        }
        return bytes;
    }
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
        int err = uv_pipe_init(server->loop, handle, listener.pipe_ipc);
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

struct pipe_connect_await : system_op {
    using promise_t = task<result<pipe>>::promise_type;

    std::unique_ptr<stream::Self, void (*)(void*)> state;
    uv_connect_t req{};
    std::string name;
    unsigned int flags = 0;
    result<pipe> outcome = std::unexpected(error());
    bool ready = true;

    pipe_connect_await(std::unique_ptr<stream::Self, void (*)(void*)> state,
                       std::string_view name,
                       pipe::options opts) : state(std::move(state)), name(name) {
        action = &on_cancel;
        if(this->name.empty()) {
            ready = false;
            outcome = std::unexpected(error::invalid_argument);
            return;
        }

        auto uv_flags = to_uv_pipe_flags(opts);
        if(!uv_flags.has_value()) {
            ready = false;
            outcome = std::unexpected(uv_flags.error());
            return;
        }
        flags = uv_flags.value();
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<pipe_connect_await*>(op);
        if(aw->state) {
            // uv_connect_t can't be cancelled; close the handle to trigger ECANCELED.
            aw->state.reset();
        }
    }

    static void on_connect(uv_connect_t* req, int status) {
        auto* aw = static_cast<pipe_connect_await*>(req->data);
        if(!aw) {
            return;
        }

        if(status < 0) {
            aw->outcome = std::unexpected(error(status));
        } else if(aw->state) {
            aw->outcome = pipe(aw->state.release());
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

        int err = uv_pipe_connect2(&req,
                                   state->as<uv_pipe_t>(),
                                   name.c_str(),
                                   name.size(),
                                   flags,
                                   on_connect);
        if(err != 0) {
            outcome = std::unexpected(error(err));
            return waiting;
        }

        return link_continuation(&waiting.promise(), location);
    }

    result<pipe> await_resume() noexcept {
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

handle_type guess_handle(int fd) {
    return to_handle_type(uv_guess_handle(fd));
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

task<std::size_t> stream::read_some(std::span<char> dst) {
    if(!self || dst.empty()) {
        co_return 0;
    }

    if(self->buffer.readable_bytes() != 0) {
        const auto available = self->buffer.readable_bytes();
        const auto to_read = std::min(dst.size(), available);
        self->buffer.read(dst.data(), to_read);
        co_return to_read;
    }

    auto stream_handle = self->as<uv_stream_t>();
    stream_handle->data = self.get();

    co_return co_await stream_read_some_await{self.get(), dst};
}

task<stream::chunk> stream::read_chunk() {
    chunk out{};
    if(!self) {
        co_return out;
    }

    auto stream_handle = self->as<uv_stream_t>();
    stream_handle->data = self.get();

    if(self->buffer.readable_bytes() == 0) {
        co_await stream_read_await{self.get()};
    }

    auto [ptr, len] = self->buffer.get_read_ptr();
    out = std::span<const char>(ptr, len);
    co_return out;
}

void stream::consume(std::size_t n) {
    if(!self) {
        return;
    }

    self->buffer.advance_read(n);
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

result<std::size_t> stream::try_write(std::span<const char> data) {
    if(!self || !self->initialized()) {
        return std::unexpected(error::invalid_argument);
    }

    if(data.empty()) {
        return std::size_t{0};
    }

    if(data.size() > static_cast<std::size_t>(std::numeric_limits<unsigned>::max())) {
        return std::unexpected(error::value_too_large_for_defined_data_type);
    }

    uv_buf_t buf = uv_buf_init(const_cast<char*>(data.data()), static_cast<unsigned>(data.size()));
    int res = uv_try_write(self->as<uv_stream_t>(), &buf, 1);
    if(res < 0) {
        return std::unexpected(error(res));
    }

    return static_cast<std::size_t>(res);
}

bool stream::readable() const noexcept {
    if(!self || !self->initialized()) {
        return false;
    }

    return uv_is_readable(self->as<uv_stream_t>()) != 0;
}

bool stream::writable() const noexcept {
    if(!self || !self->initialized()) {
        return false;
    }

    return uv_is_writable(self->as<uv_stream_t>()) != 0;
}

error stream::set_blocking(bool enabled) {
    if(!self || !self->initialized()) {
        return error::invalid_argument;
    }

    int err = uv_stream_set_blocking(self->as<uv_stream_t>(), enabled ? 1 : 0);
    if(err != 0) {
        return error(err);
    }

    return {};
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

static result<unsigned int> to_uv_pipe_flags(const pipe::options& opts) {
    unsigned int out = 0;
#ifdef UV_PIPE_NO_TRUNCATE
    if(opts.no_truncate) {
        out |= UV_PIPE_NO_TRUNCATE;
    }
#else
    if(opts.no_truncate) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

result<pipe> pipe::open(int fd, pipe::options opts, event_loop& loop) {
    auto pipe_res = create(opts, loop);
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

static int start_pipe_listen(pipe::acceptor& acc,
                             std::string_view name,
                             pipe::options opts,
                             event_loop& loop) {
    auto* self = acc.operator->();
    if(!self) {
        return error::invalid_argument.value();
    }

    auto handle = self->template as<uv_pipe_t>();

    int err = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, opts.ipc ? 1 : 0);
    if(err != 0) {
        return err;
    }

    self->init_handle();
    self->pipe_ipc = opts.ipc ? 1 : 0;

    auto uv_flags = to_uv_pipe_flags(opts);
    if(!uv_flags.has_value()) {
        return uv_flags.error().value();
    }

    if(name.empty()) {
        return error::invalid_argument.value();
    }

    err = uv_pipe_bind2(handle, name.data(), name.size(), uv_flags.value());
    if(err != 0) {
        return err;
    }

    err = uv_listen(reinterpret_cast<uv_stream_t*>(handle),
                    opts.backlog,
                    pipe_accept_await::on_connection_cb);
    return err;
}

result<pipe::acceptor> pipe::listen(std::string_view name, pipe::options opts, event_loop& loop) {
    pipe::acceptor acc(new pipe::acceptor::Self());
    int err = start_pipe_listen(acc, name, opts, loop);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return acc;
}

pipe::pipe(Self* state) noexcept : stream(state) {}

result<pipe> pipe::create(pipe::options opts, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto* handle = state->as<uv_pipe_t>();
    int errc = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, opts.ipc ? 1 : 0);
    if(errc != 0) {
        return std::unexpected(error(errc));
    }

    state->init_handle();
    return pipe(state.release());
}

task<result<pipe>> pipe::connect(std::string_view name, pipe::options opts, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto* handle = state->as<uv_pipe_t>();

    int err = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, opts.ipc ? 1 : 0);
    if(err != 0) {
        co_return std::unexpected(error(err));
    }

    state->init_handle();

    co_return co_await pipe_connect_await{std::move(state), name, opts};
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

static result<unsigned int> to_uv_tcp_bind_flags(const tcp_socket::options& opts) {
    unsigned int out = 0;
#ifdef UV_TCP_IPV6ONLY
    if(opts.ipv6_only) {
        out |= UV_TCP_IPV6ONLY;
    }
#else
    if(opts.ipv6_only) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_TCP_REUSEPORT
    if(opts.reuse_port) {
        out |= UV_TCP_REUSEPORT;
    }
#else
    if(opts.reuse_port) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

static int start_tcp_listen(tcp_socket::acceptor& acc,
                            std::string_view host,
                            int port,
                            tcp_socket::options opts,
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

    auto uv_flags = to_uv_tcp_bind_flags(opts);
    if(!uv_flags.has_value()) {
        return uv_flags.error().value();
    }

    err = uv_tcp_bind(handle, addr_ptr, uv_flags.value());
    if(err != 0) {
        return err;
    }

    err = uv_listen(reinterpret_cast<uv_stream_t*>(handle),
                    opts.backlog,
                    tcp_accept_await::on_connection_cb);
    return err;
}

result<tcp_socket::acceptor> tcp_socket::listen(std::string_view host,
                                                int port,
                                                tcp_socket::options opts,
                                                event_loop& loop) {
    tcp_socket::acceptor acc(new tcp_socket::acceptor::Self());
    int err = start_tcp_listen(acc, host, port, opts, loop);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return acc;
}

result<console> console::open(int fd, console::options opts, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    auto handle = state->as<uv_tty_t>();

    int err =
        uv_tty_init(static_cast<uv_loop_t*>(loop.handle()), handle, fd, opts.readable ? 1 : 0);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->init_handle();
    return console(state.release());
}

error console::set_mode(mode value) {
    if(!self || !self->initialized()) {
        return error::invalid_argument;
    }

    uv_tty_mode_t uv_mode = UV_TTY_MODE_NORMAL;
    switch(value) {
        case mode::normal: uv_mode = UV_TTY_MODE_NORMAL; break;
        case mode::raw: uv_mode = UV_TTY_MODE_RAW; break;
        case mode::io: uv_mode = UV_TTY_MODE_IO; break;
        case mode::raw_vt: uv_mode = UV_TTY_MODE_RAW_VT; break;
    }

    int err = uv_tty_set_mode(self->as<uv_tty_t>(), uv_mode);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error console::reset_mode() {
    int err = uv_tty_reset_mode();
    if(err != 0) {
        return error(err);
    }
    return {};
}

result<console::winsize> console::get_winsize() const {
    if(!self || !self->initialized()) {
        return std::unexpected(error::invalid_argument);
    }

    winsize out{};
    int err = uv_tty_get_winsize(self->as<uv_tty_t>(), &out.width, &out.height);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return out;
}

void console::set_vterm_state(vterm_state state) {
    auto uv_state = state == vterm_state::supported ? UV_TTY_SUPPORTED : UV_TTY_UNSUPPORTED;
    uv_tty_set_vterm_state(uv_state);
}

result<console::vterm_state> console::get_vterm_state() {
    uv_tty_vtermstate_t out = UV_TTY_UNSUPPORTED;
    int err = uv_tty_get_vterm_state(&out);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return out == UV_TTY_SUPPORTED ? vterm_state::supported : vterm_state::unsupported;
}

console::console(Self* state) noexcept : stream(state) {}

}  // namespace eventide
