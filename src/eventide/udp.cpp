#include "eventide/udp.h"

#include <cstring>
#include <optional>

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"

namespace {

struct udp_recv_tag {};

struct udp_send_tag {};

}  // namespace

namespace eventide {

udp::udp(udp&& other) noexcept :
    handle(std::move(other)), waiter(other.waiter), active(other.active),
    pending(std::move(other.pending)), buffer(std::move(other.buffer)), receiving(other.receiving),
    send_waiter(other.send_waiter), send_active(other.send_active),
    send_pending(std::move(other.send_pending)), send_inflight(other.send_inflight) {
    other.waiter = nullptr;
    other.active = nullptr;
    other.send_waiter = nullptr;
    other.send_active = nullptr;

    if(initialized()) {
        if(auto* handle = as<uv_handle_t>()) {
            handle->data = this;
        }
    }
}

udp& udp::operator=(udp&& other) noexcept {
    if(this == &other) {
        return *this;
    }

    handle::operator=(std::move(other));
    waiter = other.waiter;
    active = other.active;
    pending = std::move(other.pending);
    buffer = std::move(other.buffer);
    receiving = other.receiving;
    send_waiter = other.send_waiter;
    send_active = other.send_active;
    send_pending = std::move(other.send_pending);
    send_inflight = other.send_inflight;

    other.waiter = nullptr;
    other.active = nullptr;
    other.send_waiter = nullptr;
    other.send_active = nullptr;

    if(initialized()) {
        if(auto* handle = as<uv_handle_t>()) {
            handle->data = this;
        }
    }

    return *this;
}

static int fill_addr(std::string_view host, int port, sockaddr_storage& storage) {
    auto build = [&](auto& out, auto fn) -> int {
        return fn(std::string(host).c_str(), port, &out);
    };

    if(build(*reinterpret_cast<sockaddr_in6*>(&storage), uv_ip6_addr) == 0) {
        return AF_INET6;
    }
    if(build(*reinterpret_cast<sockaddr_in*>(&storage), uv_ip4_addr) == 0) {
        return AF_INET;
    }
    return AF_UNSPEC;
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
        uv_ip4_name(in, host, sizeof(host));
        port = ntohs(in->sin_port);
    } else if(addr->sa_family == AF_INET6) {
        auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
        uv_ip6_name(in6, host, sizeof(host));
        port = ntohs(in6->sin6_port);
    } else {
        return std::unexpected(error::invalid_argument);
    }

    out.addr = host;
    out.port = port;
    return out;
}

template <>
struct awaiter<udp_recv_tag> {
    using promise_t = task<result<udp::recv_result>>::promise_type;

    udp* self;
    result<udp::recv_result> outcome = std::unexpected(error{});

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto* u = static_cast<udp*>(handle->data);
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
        auto* u = static_cast<udp*>(handle->data);
        if(u == nullptr) {
            return;
        }

        auto deliver = [&](result<udp::recv_result>&& value) {
            if(u->waiter && u->active) {
                *u->active = std::move(value);

                auto w = u->waiter;
                u->waiter = nullptr;
                u->active = nullptr;

                w->resume();
            } else {
                u->pending.push_back(std::move(value));
            }
        };

        if(nread < 0) {
            deliver(std::unexpected(error(static_cast<int>(nread))));
            return;
        }

        udp::recv_result out{};
        out.data.assign(u->buffer.data(), u->buffer.data() + nread);
        out.flags = flags;

        if(addr != nullptr) {
            auto ep = endpoint_from_sockaddr(addr);
            if(ep.has_value()) {
                out.addr = std::move(ep->addr);
                out.port = ep->port;
            }
        }

        deliver(std::move(out));
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        self->active = &outcome;

        if(!self->receiving) {
            int err = uv_udp_recv_start(self->as<uv_udp_t>(), on_alloc, on_read);
            if(err == 0) {
                self->receiving = true;
            } else {
                outcome = std::unexpected(error(err));
                self->waiter = nullptr;
                self->active = nullptr;
                return waiting;
            }
        }

        return std::noop_coroutine();
    }

    result<udp::recv_result> await_resume() noexcept {
        self->waiter = nullptr;
        self->active = nullptr;
        return std::move(outcome);
    }
};

template <>
struct awaiter<udp_send_tag> {
    using promise_t = task<error>::promise_type;

    udp* self;
    std::vector<char> storage;
    uv_udp_send_t req{};
    std::optional<sockaddr_storage> dest;
    error result{};

    awaiter(udp* u, std::span<const char> data, std::optional<sockaddr_storage>&& d) :
        self(u), storage(data.begin(), data.end()), dest(std::move(d)) {}

    static void on_send(uv_udp_send_t* req, int status) {
        auto* handle = static_cast<uv_udp_t*>(req->handle);
        auto* u = handle ? static_cast<udp*>(handle->data) : nullptr;
        if(u == nullptr) {
            return;
        }

        u->send_inflight = false;

        auto ec = status < 0 ? error(status) : error{};

        if(u->send_waiter && u->send_active) {
            *u->send_active = ec;

            auto w = u->send_waiter;
            u->send_waiter = nullptr;
            u->send_active = nullptr;

            w->resume();
        } else {
            u->send_pending = ec;
        }
    }

    bool await_ready() noexcept {
        if(self->send_pending.has_value()) {
            result = *self->send_pending;
            self->send_pending.reset();
            return true;
        }
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        if(self->send_waiter != nullptr || self->send_inflight) {
            result = error::connection_already_in_progress;
            return waiting;
        }

        self->send_waiter = waiting ? &waiting.promise() : nullptr;
        self->send_active = &result;

        uv_buf_t buf = uv_buf_init(storage.empty() ? nullptr : storage.data(),
                                   static_cast<unsigned>(storage.size()));

        const sockaddr* addr =
            dest.has_value() ? reinterpret_cast<const sockaddr*>(&dest.value()) : nullptr;

        int err = uv_udp_send(&req, self->as<uv_udp_t>(), &buf, 1, addr, on_send);
        if(err != 0) {
            result = error(err);
            self->send_waiter = nullptr;
            self->send_active = nullptr;
            return waiting;
        }

        self->send_inflight = true;
        return std::noop_coroutine();
    }

    error await_resume() noexcept {
        self->send_waiter = nullptr;
        self->send_active = nullptr;
        return result;
    }
};

result<udp> udp::create(event_loop& loop) {
    udp u(sizeof(uv_udp_t));
    u.buffer.resize(64 * 1024);
    auto handle = u.as<uv_udp_t>();
    int err = uv_udp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    u.mark_initialized();
    handle->data = &u;
    return u;
}

result<udp> udp::create(event_loop& loop, unsigned int flags) {
    udp u(sizeof(uv_udp_t));
    u.buffer.resize(64 * 1024);
    auto handle = u.as<uv_udp_t>();
    int err = uv_udp_init_ex(static_cast<uv_loop_t*>(loop.handle()), handle, flags);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    u.mark_initialized();
    handle->data = &u;
    return u;
}

result<udp> udp::open(event_loop& loop, int fd) {
    udp u(sizeof(uv_udp_t));
    u.buffer.resize(64 * 1024);
    auto handle = u.as<uv_udp_t>();
    int err = uv_udp_init(static_cast<uv_loop_t*>(loop.handle()), handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    err = uv_udp_open(handle, fd);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    u.mark_initialized();
    handle->data = &u;
    return u;
}

error udp::bind(std::string_view host, int port, unsigned flags) {
    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        return error::invalid_argument;
    }

    auto handle = as<uv_udp_t>();
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&storage);
    int err = uv_udp_bind(handle, addr, flags);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::connect(std::string_view host, int port) {
    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        return error::invalid_argument;
    }

    auto handle = as<uv_udp_t>();
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&storage);
    int err = uv_udp_connect(handle, addr);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::disconnect() {
    auto handle = as<uv_udp_t>();
    int err = uv_udp_connect(handle, nullptr);
    if(err != 0) {
        return error(err);
    }

    return {};
}

task<error> udp::send(std::span<const char> data, std::string_view host, int port) {
    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        co_return error::invalid_argument;
    }

    co_return co_await awaiter<udp_send_tag>{this, data, std::optional<sockaddr_storage>(storage)};
}

task<error> udp::send(std::span<const char> data) {
    co_return co_await awaiter<udp_send_tag>{this, data, std::nullopt};
}

error udp::try_send(std::span<const char> data, std::string_view host, int port) {
    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        return error::invalid_argument;
    }

    uv_buf_t buf =
        uv_buf_init(const_cast<char*>(data.data()), static_cast<unsigned int>(data.size()));
    int err = uv_udp_try_send(as<uv_udp_t>(), &buf, 1, reinterpret_cast<const sockaddr*>(&storage));
    if(err < 0) {
        return error(err);
    }

    return {};
}

error udp::try_send(std::span<const char> data) {
    uv_buf_t buf =
        uv_buf_init(const_cast<char*>(data.data()), static_cast<unsigned int>(data.size()));
    int err = uv_udp_try_send(as<uv_udp_t>(), &buf, 1, nullptr);
    if(err < 0) {
        return error(err);
    }

    return {};
}

error udp::stop_recv() {
    auto handle = as<uv_udp_t>();
    int err = uv_udp_recv_stop(handle);
    if(err != 0) {
        return error(err);
    }

    receiving = false;
    return {};
}

task<result<udp::recv_result>> udp::recv() {
    if(!pending.empty()) {
        auto out = std::move(pending.front());
        pending.pop_front();
        co_return out;
    }

    if(waiter != nullptr) {
        co_return std::unexpected(error::connection_already_in_progress);
    }

    co_return co_await awaiter<udp_recv_tag>{this};
}

result<udp::endpoint> udp::getsockname() const {
    sockaddr_storage storage{};
    int len = sizeof(storage);
    int err = uv_udp_getsockname(as<uv_udp_t>(), reinterpret_cast<sockaddr*>(&storage), &len);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&storage));
}

result<udp::endpoint> udp::getpeername() const {
    sockaddr_storage storage{};
    int len = sizeof(storage);
    int err = uv_udp_getpeername(as<uv_udp_t>(), reinterpret_cast<sockaddr*>(&storage), &len);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&storage));
}

error udp::set_membership(std::string_view multicast_addr,
                          std::string_view interface_addr,
                          membership m) {
    int err = uv_udp_set_membership(as<uv_udp_t>(),
                                    std::string(multicast_addr).c_str(),
                                    std::string(interface_addr).c_str(),
                                    m == membership::join ? UV_JOIN_GROUP : UV_LEAVE_GROUP);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_source_membership(std::string_view multicast_addr,
                                 std::string_view interface_addr,
                                 std::string_view source_addr,
                                 membership m) {
    int err = uv_udp_set_source_membership(as<uv_udp_t>(),
                                           std::string(multicast_addr).c_str(),
                                           std::string(interface_addr).c_str(),
                                           std::string(source_addr).c_str(),
                                           m == membership::join ? UV_JOIN_GROUP : UV_LEAVE_GROUP);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_multicast_loop(bool on) {
    int err = uv_udp_set_multicast_loop(as<uv_udp_t>(), on ? 1 : 0);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_multicast_ttl(int ttl) {
    int err = uv_udp_set_multicast_ttl(as<uv_udp_t>(), ttl);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_multicast_interface(std::string_view interface_addr) {
    int err = uv_udp_set_multicast_interface(as<uv_udp_t>(), std::string(interface_addr).c_str());
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_broadcast(bool on) {
    int err = uv_udp_set_broadcast(as<uv_udp_t>(), on ? 1 : 0);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_ttl(int ttl) {
    int err = uv_udp_set_ttl(as<uv_udp_t>(), ttl);
    if(err != 0) {
        return error(err);
    }

    return {};
}

bool udp::using_recvmmsg() const {
    return uv_udp_using_recvmmsg(as<uv_udp_t>()) != 0;
}

std::size_t udp::send_queue_size() const {
    return uv_udp_get_send_queue_size(as<uv_udp_t>());
}

std::size_t udp::send_queue_count() const {
    return uv_udp_get_send_queue_count(as<uv_udp_t>());
}

}  // namespace eventide
