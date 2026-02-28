#include "eventide/async/udp.h"

#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include "libuv.h"
#include "eventide/async/error.h"
#include "eventide/async/loop.h"

namespace eventide {

static result<udp::endpoint> endpoint_from_sockaddr(const sockaddr* addr);

struct udp::Self : uv_handle<udp::Self, uv_udp_t> {
    uv_udp_t handle{};
    system_op* waiter = nullptr;
    result<udp::recv_result>* active = nullptr;
    std::deque<result<udp::recv_result>> pending;
    std::vector<char> buffer;
    bool receiving = false;

    system_op* send_waiter = nullptr;
    error* send_active = nullptr;
    std::optional<error> send_pending;
    bool send_inflight = false;

    Self() {
        handle.data = this;
    }
};

namespace {

constexpr std::size_t udp_recv_buffer_size = 64 * 1024;

static udp::Self::pointer make_udp_self() {
    auto self = udp::Self::make();
    self->buffer.resize(udp_recv_buffer_size);
    return self;
}

static result<unsigned int> to_uv_udp_init_flags(const udp::create_options& options) {
    unsigned int out = 0;
#ifdef UV_UDP_IPV6ONLY
    if(options.ipv6_only) {
        out |= UV_UDP_IPV6ONLY;
    }
#else
    if(options.ipv6_only) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_UDP_RECVMMSG
    if(options.recvmmsg) {
        out |= UV_UDP_RECVMMSG;
    }
#else
    if(options.recvmmsg) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

static result<unsigned int> to_uv_udp_bind_flags(const udp::bind_options& options) {
    unsigned int out = 0;
#ifdef UV_UDP_IPV6ONLY
    if(options.ipv6_only) {
        out |= UV_UDP_IPV6ONLY;
    }
#else
    if(options.ipv6_only) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_UDP_REUSEADDR
    if(options.reuse_addr) {
        out |= UV_UDP_REUSEADDR;
    }
#else
    if(options.reuse_addr) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_UDP_REUSEPORT
    if(options.reuse_port) {
        out |= UV_UDP_REUSEPORT;
    }
#else
    if(options.reuse_port) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

static udp::recv_flags to_udp_recv_flags(unsigned flags) {
    udp::recv_flags out{};
#ifdef UV_UDP_PARTIAL
    if((flags & UV_UDP_PARTIAL) != 0) {
        out.partial = true;
    }
#endif
#ifdef UV_UDP_MMSG_CHUNK
    if((flags & UV_UDP_MMSG_CHUNK) != 0) {
        out.mmsg_chunk = true;
    }
#endif
    return out;
}

struct udp_recv_await : system_op {
    using promise_t = task<result<udp::recv_result>>::promise_type;

    // UDP socket self used to register waiter and manage recv lifecycle.
    udp::Self* self;
    // Result slot written by on_read() and returned from await_resume().
    result<udp::recv_result> outcome = std::unexpected(error());

    explicit udp_recv_await(udp::Self* socket) : self(socket) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        detail::cancel_and_complete<udp_recv_await>(op, [](auto& aw) {
            if(aw.self && aw.self->receiving) {
                uv::udp_recv_stop(aw.self->handle);
                aw.self->receiving = false;
            }
            detail::clear_waiter_active(aw.self, &udp::Self::waiter, &udp::Self::active);
        });
    }

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto* u = static_cast<udp::Self*>(handle->data);
        if(u == nullptr) {
            buf->base = nullptr;
            buf->len = 0;
            return;
        }

        buf->base = u->buffer.data();
        buf->len = u->buffer.size();
    }

    static void on_read(uv_udp_t* handle,
                        ssize_t nread,
                        const uv_buf_t*,
                        const struct sockaddr* addr,
                        unsigned flags) {
        auto* u = static_cast<udp::Self*>(handle->data);
        if(u == nullptr) {
            return;
        }

        auto deliver = [&](result<udp::recv_result>&& value) {
            detail::deliver_or_queue(u->waiter, u->active, u->pending, std::move(value));
        };

        if(nread < 0) {
            detail::mark_cancelled_if(u->waiter, nread);
            deliver(std::unexpected(detail::status_to_error(nread)));
            return;
        }

        udp::recv_result out{};
        out.data.assign(u->buffer.data(), u->buffer.data() + nread);
        out.flags = to_udp_recv_flags(flags);

        if(addr != nullptr) {
            auto ep = endpoint_from_sockaddr(addr);
            if(ep) {
                out.addr = std::move(ep->addr);
                out.port = ep->port;
            }
        }

        deliver(std::move(out));
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

        if(!self->receiving) {
            auto err = uv::udp_recv_start(self->handle, on_alloc, on_read);
            if(!err) {
                self->receiving = true;
            } else {
                outcome = std::unexpected(err);
                self->waiter = nullptr;
                self->active = nullptr;
                return waiting;
            }
        }

        return link_continuation(&waiting.promise(), location);
    }

    result<udp::recv_result> await_resume() noexcept {
        if(self) {
            self->waiter = nullptr;
            self->active = nullptr;
        }
        return std::move(outcome);
    }
};

struct udp_send_await : system_op {
    using promise_t = task<error>::promise_type;

    // UDP socket self that owns send waiter and inflight flags.
    udp::Self* self;
    // Owns outbound bytes until on_send() runs.
    std::vector<char> storage;
    // libuv send request; req.handle gives us the socket on completion.
    uv_udp_send_t req{};
    // Optional destination for unconnected sockets.
    std::optional<sockaddr_storage> dest;
    // Completion status returned from await_resume().
    error result;

    udp_send_await(udp::Self* u, std::span<const char> data, std::optional<sockaddr_storage>&& d) :
        self(u), storage(data.begin(), data.end()), dest(std::move(d)) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<udp_send_await*>(op);
        if(!aw->self) {
            return;
        }
        // uv_udp_send_t is not cancellable via uv_cancel().
        // Keep the request in-flight and wait for on_send() to retire it.
    }

    static void on_send(uv_udp_send_t* req, int status) {
        auto* handle = static_cast<uv_udp_t*>(req->handle);
        auto* u = handle ? static_cast<udp::Self*>(handle->data) : nullptr;
        if(u == nullptr) {
            return;
        }

        u->send_inflight = false;

        detail::mark_cancelled_if(u->send_waiter, status);

        auto ec = detail::status_to_error(status);

        detail::deliver_or_store(u->send_waiter, u->send_active, u->send_pending, std::move(ec));
    }

    bool await_ready() noexcept {
        if(self && self->send_pending.has_value()) {
            result = *self->send_pending;
            self->send_pending.reset();
            return true;
        }
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            result = error::invalid_argument;
            return waiting;
        }

        if(self->send_waiter != nullptr || self->send_inflight) {
            result = error::connection_already_in_progress;
            return waiting;
        }

        self->send_waiter = this;
        self->send_active = &result;

        uv_buf_t buf = uv::buf_init(storage.empty() ? nullptr : storage.data(),
                                    static_cast<unsigned>(storage.size()));

        const sockaddr* addr =
            dest.has_value() ? reinterpret_cast<const sockaddr*>(&dest.value()) : nullptr;

        auto err =
            uv::udp_send(req, self->handle, std::span<const uv_buf_t>{&buf, 1}, addr, on_send);
        if(err) {
            result = err;
            self->send_waiter = nullptr;
            self->send_active = nullptr;
            return waiting;
        }

        self->send_inflight = true;
        return link_continuation(&waiting.promise(), location);
    }

    error await_resume() noexcept {
        if(self) {
            self->send_waiter = nullptr;
            self->send_active = nullptr;
        }
        return result;
    }
};

}  // namespace

udp::udp() noexcept = default;

udp::udp(unique_handle<Self> self) noexcept : self(std::move(self)) {}

udp::~udp() = default;

udp::udp(udp&& other) noexcept = default;

udp& udp::operator=(udp&& other) noexcept = default;

udp::Self* udp::operator->() noexcept {
    return self.get();
}

static result<udp::endpoint> endpoint_from_sockaddr(const sockaddr* addr) {
    if(addr == nullptr) {
        return std::unexpected(error::invalid_argument);
    }

    udp::endpoint out{};
    char host[INET6_ADDRSTRLEN]{};
    int port = 0;
    if(addr->sa_family == AF_INET) {
        auto* in = reinterpret_cast<const sockaddr_in*>(addr);
        if(auto err = uv::ip4_name(*in, host, sizeof(host))) {
            return std::unexpected(err);
        }
        port = ntohs(in->sin_port);
    } else if(addr->sa_family == AF_INET6) {
        auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
        if(auto err = uv::ip6_name(*in6, host, sizeof(host))) {
            return std::unexpected(err);
        }
        port = ntohs(in6->sin6_port);
    } else {
        return std::unexpected(error::invalid_argument);
    }

    out.addr = host;
    out.port = port;
    return out;
}

result<udp> udp::create(event_loop& loop) {
    auto self = make_udp_self();
    if(auto err = uv::udp_init(loop, self->handle)) {
        return std::unexpected(err);
    }

    self->init_handle();
    return udp(std::move(self));
}

result<udp> udp::create(create_options options, event_loop& loop) {
    auto self = make_udp_self();
    auto uv_flags = to_uv_udp_init_flags(options);
    if(!uv_flags) {
        return std::unexpected(uv_flags.error());
    }

    if(auto err = uv::udp_init_ex(loop, self->handle, uv_flags.value())) {
        return std::unexpected(err);
    }

    self->init_handle();
    return udp(std::move(self));
}

result<udp> udp::open(int fd, event_loop& loop) {
    auto self = make_udp_self();
    if(auto err = uv::udp_init(loop, self->handle)) {
        return std::unexpected(err);
    }

    self->init_handle();

    if(auto err = uv::udp_open(self->handle, fd)) {
        return std::unexpected(err);
    }

    return udp(std::move(self));
}

error udp::bind(std::string_view host, int port, bind_options options) {
    if(!self) {
        return error::invalid_argument;
    }

    auto uv_flags = to_uv_udp_bind_flags(options);
    if(!uv_flags) {
        return uv_flags.error();
    }

    auto resolved = detail::resolve_addr(host, port);
    if(!resolved) {
        return resolved.error();
    }

    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&resolved->storage);
    if(auto err = uv::udp_bind(self->handle, addr, uv_flags.value())) {
        return err;
    }

    return {};
}

error udp::connect(std::string_view host, int port) {
    if(!self) {
        return error::invalid_argument;
    }

    auto resolved = detail::resolve_addr(host, port);
    if(!resolved) {
        return resolved.error();
    }

    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&resolved->storage);
    if(auto err = uv::udp_connect(self->handle, addr)) {
        return err;
    }

    return {};
}

error udp::disconnect() {
    if(!self) {
        return error::invalid_argument;
    }

    if(auto err = uv::udp_connect(self->handle, nullptr)) {
        return err;
    }

    return {};
}

task<error> udp::send(std::span<const char> data, std::string_view host, int port) {
    if(!self) {
        co_return error::invalid_argument;
    }

    auto resolved = detail::resolve_addr(host, port);
    if(!resolved) {
        co_return resolved.error();
    }

    co_return co_await udp_send_await{self.get(),
                                      data,
                                      std::optional<sockaddr_storage>(resolved->storage)};
}

task<error> udp::send(std::span<const char> data) {
    if(!self) {
        co_return error::invalid_argument;
    }

    co_return co_await udp_send_await{self.get(), data, std::nullopt};
}

error udp::try_send(std::span<const char> data, std::string_view host, int port) {
    if(!self) {
        return error::invalid_argument;
    }

    auto resolved = detail::resolve_addr(host, port);
    if(!resolved) {
        return resolved.error();
    }

    uv_buf_t buf =
        uv::buf_init(const_cast<char*>(data.data()), static_cast<unsigned int>(data.size()));
    auto sent = uv::udp_try_send(self->handle,
                                 std::span<const uv_buf_t>{&buf, 1},
                                 reinterpret_cast<const sockaddr*>(&resolved->storage));
    if(!sent) {
        return sent.error();
    }

    return {};
}

error udp::try_send(std::span<const char> data) {
    if(!self) {
        return error::invalid_argument;
    }

    uv_buf_t buf =
        uv::buf_init(const_cast<char*>(data.data()), static_cast<unsigned int>(data.size()));
    auto sent = uv::udp_try_send(self->handle, std::span<const uv_buf_t>{&buf, 1}, nullptr);
    if(!sent) {
        return sent.error();
    }

    return {};
}

error udp::stop_recv() {
    if(!self) {
        return error::invalid_argument;
    }

    uv::udp_recv_stop(self->handle);
    self->receiving = false;
    return {};
}

task<result<udp::recv_result>> udp::recv() {
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

    co_return co_await udp_recv_await{self.get()};
}

result<udp::endpoint> udp::getsockname() const {
    if(!self) {
        return std::unexpected(error::invalid_argument);
    }

    sockaddr_storage storage{};
    int len = sizeof(storage);
    if(auto err = uv::udp_getsockname(self->handle, *reinterpret_cast<sockaddr*>(&storage), len)) {
        return std::unexpected(err);
    }

    return endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&storage));
}

result<udp::endpoint> udp::getpeername() const {
    if(!self) {
        return std::unexpected(error::invalid_argument);
    }

    sockaddr_storage storage{};
    int len = sizeof(storage);
    if(auto err = uv::udp_getpeername(self->handle, *reinterpret_cast<sockaddr*>(&storage), len)) {
        return std::unexpected(err);
    }

    return endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&storage));
}

error udp::set_membership(std::string_view multicast_addr,
                          std::string_view interface_addr,
                          membership m) {
    if(!self) {
        return error::invalid_argument;
    }

    std::string multicast_storage(multicast_addr);
    std::string interface_storage(interface_addr);
    if(auto err = uv::udp_set_membership(self->handle,
                                         multicast_storage.c_str(),
                                         interface_storage.c_str(),
                                         m == membership::join ? UV_JOIN_GROUP : UV_LEAVE_GROUP)) {
        return err;
    }

    return {};
}

error udp::set_source_membership(std::string_view multicast_addr,
                                 std::string_view interface_addr,
                                 std::string_view source_addr,
                                 membership m) {
    if(!self) {
        return error::invalid_argument;
    }

    std::string multicast_storage(multicast_addr);
    std::string interface_storage(interface_addr);
    std::string source_storage(source_addr);
    if(auto err =
           uv::udp_set_source_membership(self->handle,
                                         multicast_storage.c_str(),
                                         interface_storage.c_str(),
                                         source_storage.c_str(),
                                         m == membership::join ? UV_JOIN_GROUP : UV_LEAVE_GROUP)) {
        return err;
    }

    return {};
}

error udp::set_multicast_loop(bool on) {
    if(!self) {
        return error::invalid_argument;
    }

    if(auto err = uv::udp_set_multicast_loop(self->handle, on)) {
        return err;
    }

    return {};
}

error udp::set_multicast_ttl(int ttl) {
    if(!self) {
        return error::invalid_argument;
    }

    if(auto err = uv::udp_set_multicast_ttl(self->handle, ttl)) {
        return err;
    }

    return {};
}

error udp::set_multicast_interface(std::string_view interface_addr) {
    if(!self) {
        return error::invalid_argument;
    }

    std::string interface_storage(interface_addr);
    if(auto err = uv::udp_set_multicast_interface(self->handle, interface_storage.c_str())) {
        return err;
    }

    return {};
}

error udp::set_broadcast(bool on) {
    if(!self) {
        return error::invalid_argument;
    }

    if(auto err = uv::udp_set_broadcast(self->handle, on)) {
        return err;
    }

    return {};
}

error udp::set_ttl(int ttl) {
    if(!self) {
        return error::invalid_argument;
    }

    if(auto err = uv::udp_set_ttl(self->handle, ttl)) {
        return err;
    }

    return {};
}

bool udp::using_recvmmsg() const {
    if(!self) {
        return false;
    }

    return uv::udp_using_recvmmsg(self->handle);
}

std::size_t udp::send_queue_size() const {
    if(!self) {
        return 0;
    }

    return uv::udp_get_send_queue_size(self->handle);
}

std::size_t udp::send_queue_count() const {
    if(!self) {
        return 0;
    }

    return uv::udp_get_send_queue_count(self->handle);
}

}  // namespace eventide
