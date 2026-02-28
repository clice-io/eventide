#include <memory>
#include <type_traits>
#include <utility>

#include "stream_internal.h"
#include "eventide/async/loop.h"

namespace eventide {

namespace {

template <typename T>
constexpr inline bool always_false_v = false;

result<unsigned int> to_uv_pipe_flags(const pipe::options& opts) {
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

result<unsigned int> to_uv_pipe_connect_flags(const pipe::options& opts) {
    return to_uv_pipe_flags(opts);
}

result<unsigned int> to_uv_tcp_bind_flags(const tcp_socket::options& opts) {
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

template <typename Stream>
struct accept_await : system_op {
    using promise_t = task<result<Stream>>::promise_type;
    using self_t = typename acceptor<Stream>::Self;

    // Acceptor state used for waiter registration and pending queueing.
    self_t* self;
    // Result slot populated by connection callbacks.
    result<Stream> outcome = std::unexpected(error());

    explicit accept_await(self_t* acceptor) : self(acceptor) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        detail::cancel_and_complete<accept_await<Stream>>(op, [](auto& aw) {
            detail::clear_waiter_active(aw.self, &self_t::waiter, &self_t::active);
        });
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

    result<Stream> await_resume() noexcept {
        if(self) {
            self->waiter = nullptr;
            self->active = nullptr;
        }
        return std::move(outcome);
    }
};

template <typename Stream>
void on_connection(uv_stream_t* server, int status) {
    using self_t = typename acceptor<Stream>::Self;

    auto* listener = static_cast<self_t*>(server ? server->data : nullptr);
    if(listener == nullptr) {
        return;
    }

    auto deliver = [&](result<Stream>&& value) {
        detail::deliver_or_queue(listener->waiter,
                                 listener->active,
                                 listener->pending,
                                 std::move(value));
    };

    if(status < 0) {
        deliver(std::unexpected(detail::status_to_error(status)));
        return;
    }

    auto state = stream::Self::make();

    if constexpr(std::is_same_v<Stream, pipe>) {
        auto& handle = state->pipe;
        auto err = uv::pipe_init(*server->loop, handle, listener->pipe_ipc);
        if(!err) {
            state->init_handle();
            err = uv::accept(*server, handle);
        }

        if(err) {
            deliver(std::unexpected(err));
        } else {
            deliver(pipe(state.release()));
        }
    } else if constexpr(std::is_same_v<Stream, tcp_socket>) {
        auto& handle = state->tcp;
        auto err = uv::tcp_init(*server->loop, handle);
        if(!err) {
            state->init_handle();
            err = uv::accept(*server, handle);
        }

        if(err) {
            deliver(std::unexpected(err));
        } else {
            deliver(tcp_socket(state.release()));
        }
    } else {
        static_assert(always_false_v<Stream>, "unsupported accept stream type");
    }
}

template <typename Stream>
struct connect_await : system_op {
    using promise_t = task<result<Stream>>::promise_type;
    using state_ptr = stream::Self::pointer;

    // Candidate stream state; reset on cancel to close the handle.
    state_ptr state;
    // libuv connect request; req.data points back to this awaiter.
    uv_connect_t req{};
    // Pipe name kept alive for uv_pipe_connect2().
    std::string name;
    // Pipe connect flags.
    unsigned int flags = 0;
    // Resolved peer address for uv_tcp_connect().
    sockaddr_storage addr{};
    // Result slot returned from await_resume().
    result<Stream> outcome = std::unexpected(error());
    // Constructor-level validation flag.
    bool ready = true;

    connect_await(state_ptr state, std::string_view name, pipe::options opts) :
        state(std::move(state)), name(name) {
        action = &on_cancel;

        if constexpr(std::is_same_v<Stream, pipe>) {
            if(this->name.empty()) {
                ready = false;
                outcome = std::unexpected(error::invalid_argument);
                return;
            }

            auto uv_flags = to_uv_pipe_connect_flags(opts);
            if(!uv_flags) {
                ready = false;
                outcome = std::unexpected(uv_flags.error());
                return;
            }
            flags = uv_flags.value();
        } else {
            static_assert(always_false_v<Stream>, "pipe constructor requires Stream=pipe");
        }
    }

    connect_await(state_ptr state, std::string_view host, int port) : state(std::move(state)) {
        action = &on_cancel;

        if constexpr(std::is_same_v<Stream, tcp_socket>) {
            auto resolved = detail::resolve_addr(host, port);
            if(!resolved) {
                ready = false;
                outcome = std::unexpected(resolved.error());
                return;
            }
            addr = resolved->storage;
        } else {
            static_assert(always_false_v<Stream>, "tcp constructor requires Stream=tcp_socket");
        }
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<connect_await*>(op);
        if(aw->state) {
            // uv_connect_t can't be cancelled; close handle to trigger UV_ECANCELED callback.
            aw->state.reset();
        }
    }

    static void on_connect(uv_connect_t* req, int status) {
        auto* aw = static_cast<connect_await*>(req->data);
        if(!aw) {
            return;
        }

        detail::mark_cancelled_if(aw, status);

        if(status < 0) {
            aw->outcome = std::unexpected(detail::status_to_error(status));
        } else if(aw->state) {
            if constexpr(std::is_same_v<Stream, pipe>) {
                aw->outcome = pipe(aw->state.release());
            } else if constexpr(std::is_same_v<Stream, tcp_socket>) {
                aw->outcome = tcp_socket(aw->state.release());
            } else {
                static_assert(always_false_v<Stream>, "unsupported connect stream type");
            }
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

        error err{};
        if constexpr(std::is_same_v<Stream, pipe>) {
            err = uv::pipe_connect2(req, state->pipe, name.c_str(), name.size(), flags, on_connect);
        } else if constexpr(std::is_same_v<Stream, tcp_socket>) {
            err = uv::tcp_connect(req,
                                  state->tcp,
                                  reinterpret_cast<const sockaddr*>(&addr),
                                  on_connect);
        } else {
            static_assert(always_false_v<Stream>, "unsupported connect stream type");
        }

        if(err) {
            outcome = std::unexpected(err);
            return waiting;
        }

        return link_continuation(&waiting.promise(), location);
    }

    result<Stream> await_resume() noexcept {
        return std::move(outcome);
    }
};

}  // namespace

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

    co_return co_await accept_await<Stream>{self.get()};
}

template <typename Stream>
error acceptor<Stream>::stop() {
    if(!self) {
        return error::invalid_argument;
    }

    result<Stream> error_result = std::unexpected(error::operation_aborted);
    detail::deliver_or_queue(self->waiter, self->active, self->pending, std::move(error_result));

    return {};
}

template <typename Stream>
acceptor<Stream>::acceptor(Self* state) noexcept : self(state, Self::destroy) {}

template class acceptor<pipe>;
template class acceptor<tcp_socket>;

result<pipe> pipe::open(int fd, pipe::options opts, event_loop& loop) {
    auto pipe_res = create(opts, loop);
    if(!pipe_res) {
        return std::unexpected(pipe_res.error());
    }

    auto& handle = pipe_res->self->pipe;
    if(auto err = uv::pipe_open(handle, fd)) {
        return std::unexpected(err);
    }

    return std::move(*pipe_res);
}

result<pipe::acceptor> pipe::listen(std::string_view name, pipe::options opts, event_loop& loop) {
    auto state = pipe::acceptor::Self::make();
    if(auto err = uv::pipe_init(loop, state->pipe, opts.ipc ? 1 : 0)) {
        return std::unexpected(err);
    }
    state->init_handle();

    auto& acc = *state;
    acc.pipe_ipc = opts.ipc ? 1 : 0;
    auto& handle = acc.pipe;

    auto uv_flags = to_uv_pipe_flags(opts);
    if(!uv_flags) {
        return std::unexpected(uv_flags.error());
    }

    if(name.empty()) {
        return std::unexpected(error::invalid_argument);
    }

    if(auto err = uv::pipe_bind2(handle, name.data(), name.size(), uv_flags.value())) {
        return std::unexpected(err);
    }

    if(auto err = uv::listen(handle, opts.backlog, on_connection<pipe>)) {
        return std::unexpected(err);
    }

    return pipe::acceptor(state.release());
}

pipe::pipe(Self* state) noexcept : stream(state) {}

result<pipe> pipe::create(pipe::options opts, event_loop& loop) {
    auto state = Self::make();
    if(auto err = uv::pipe_init(loop, state->pipe, opts.ipc ? 1 : 0)) {
        return std::unexpected(err);
    }
    state->init_handle();

    return pipe(state.release());
}

task<result<pipe>> pipe::connect(std::string_view name, pipe::options opts, event_loop& loop) {
    auto state = Self::make();
    if(auto err = uv::pipe_init(loop, state->pipe, opts.ipc ? 1 : 0)) {
        co_return std::unexpected(err);
    }
    state->init_handle();

    co_return co_await connect_await<pipe>{std::move(state), name, opts};
}

tcp_socket::tcp_socket(Self* state) noexcept : stream(state) {}

result<tcp_socket> tcp_socket::open(int fd, event_loop& loop) {
    auto state = Self::make();
    if(auto err = uv::tcp_init(loop, state->tcp)) {
        return std::unexpected(err);
    }
    state->init_handle();

    if(auto err = uv::tcp_open(state->tcp, fd)) {
        return std::unexpected(err);
    }

    return tcp_socket(state.release());
}

task<result<tcp_socket>> tcp_socket::connect(std::string_view host, int port, event_loop& loop) {
    auto state = Self::make();
    if(auto err = uv::tcp_init(loop, state->tcp)) {
        co_return std::unexpected(err);
    }
    state->init_handle();

    co_return co_await connect_await<tcp_socket>{std::move(state), host, port};
}

result<tcp_socket::acceptor> tcp_socket::listen(std::string_view host,
                                                int port,
                                                tcp_socket::options opts,
                                                event_loop& loop) {
    auto state = tcp_socket::acceptor::Self::make();
    if(auto err = uv::tcp_init(loop, state->tcp)) {
        return std::unexpected(err);
    }
    state->init_handle();

    auto& acc = *state;
    auto& handle = acc.tcp;

    auto resolved = detail::resolve_addr(host, port);
    if(!resolved) {
        return std::unexpected(resolved.error());
    }

    ::sockaddr* addr_ptr = reinterpret_cast<sockaddr*>(&resolved->storage);

    auto uv_flags = to_uv_tcp_bind_flags(opts);
    if(!uv_flags) {
        return std::unexpected(uv_flags.error());
    }

    if(auto err = uv::tcp_bind(handle, addr_ptr, uv_flags.value())) {
        return std::unexpected(err);
    }

    if(auto err = uv::listen(handle, opts.backlog, on_connection<tcp_socket>)) {
        return std::unexpected(err);
    }

    return tcp_socket::acceptor(state.release());
}

}  // namespace eventide
