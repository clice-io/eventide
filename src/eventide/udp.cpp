#include "eventide/udp.h"

#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"

namespace eventide {

static int fill_addr(std::string_view host, int port, sockaddr_storage& storage);

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

static result<unsigned int> to_uv_udp_init_flags(udp::create_flags flags) {
    unsigned int out = 0;
#ifdef UV_UDP_IPV6ONLY
    if(has_flag(flags, udp::create_flags::ipv6_only)) {
        out |= UV_UDP_IPV6ONLY;
    }
#else
    if(has_flag(flags, udp::create_flags::ipv6_only)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_UDP_RECVMMSG
    if(has_flag(flags, udp::create_flags::recvmmsg)) {
        out |= UV_UDP_RECVMMSG;
    }
#else
    if(has_flag(flags, udp::create_flags::recvmmsg)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

static result<unsigned int> to_uv_udp_bind_flags(udp::bind_flags flags) {
    unsigned int out = 0;
#ifdef UV_UDP_IPV6ONLY
    if(has_flag(flags, udp::bind_flags::ipv6_only)) {
        out |= UV_UDP_IPV6ONLY;
    }
#else
    if(has_flag(flags, udp::bind_flags::ipv6_only)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_UDP_REUSEADDR
    if(has_flag(flags, udp::bind_flags::reuse_addr)) {
        out |= UV_UDP_REUSEADDR;
    }
#else
    if(has_flag(flags, udp::bind_flags::reuse_addr)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
#ifdef UV_UDP_REUSEPORT
    if(has_flag(flags, udp::bind_flags::reuse_port)) {
        out |= UV_UDP_REUSEPORT;
    }
#else
    if(has_flag(flags, udp::bind_flags::reuse_port)) {
        return std::unexpected(error::function_not_implemented);
    }
#endif
    return out;
}

static udp::recv_flags to_udp_recv_flags(unsigned flags) {
    auto out = udp::recv_flags::none;
#ifdef UV_UDP_PARTIAL
    if((flags & UV_UDP_PARTIAL) != 0) {
        out |= udp::recv_flags::partial;
    }
#endif
#ifdef UV_UDP_MMSG_CHUNK
    if((flags & UV_UDP_MMSG_CHUNK) != 0) {
        out |= udp::recv_flags::mmsg_chunk;
    }
#endif
    return out;
}

struct udp_recv_await : system_op {
    using promise_t = task<result<udp::recv_result>>::promise_type;

    udp::Self* self;
    result<udp::recv_result> outcome = std::unexpected(error{});

    explicit udp_recv_await(udp::Self* socket) : self(socket) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<udp_recv_await*>(op);
        if(aw->self) {
            if(aw->self->receiving) {
                uv_udp_recv_stop(&aw->self->handle);
                aw->self->receiving = false;
            }
            aw->self->waiter = nullptr;
            aw->self->active = nullptr;
        }
        aw->complete();
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
            if(u->waiter && u->active) {
                *u->active = std::move(value);

                auto w = u->waiter;
                u->waiter = nullptr;
                u->active = nullptr;

                w->complete();
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
        out.flags = to_udp_recv_flags(flags);

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

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }

        self->waiter = this;
        self->active = &outcome;

        if(!self->receiving) {
            int err = uv_udp_recv_start(&self->handle, on_alloc, on_read);
            if(err == 0) {
                self->receiving = true;
            } else {
                outcome = std::unexpected(error(err));
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

    udp::Self* self;
    std::vector<char> storage;
    uv_udp_send_t req{};
    std::optional<sockaddr_storage> dest;
    error result{};

    udp_send_await(udp::Self* u, std::span<const char> data, std::optional<sockaddr_storage>&& d) :
        self(u), storage(data.begin(), data.end()), dest(std::move(d)) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<udp_send_await*>(op);
        if(aw->self) {
            uv_cancel(reinterpret_cast<uv_req_t*>(&aw->req));
        }
    }

    static void on_send(uv_udp_send_t* req, int status) {
        auto* handle = static_cast<uv_udp_t*>(req->handle);
        auto* u = handle ? static_cast<udp::Self*>(handle->data) : nullptr;
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

            w->complete();
        } else {
            u->send_pending = ec;
        }
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

        uv_buf_t buf = uv_buf_init(storage.empty() ? nullptr : storage.data(),
                                   static_cast<unsigned>(storage.size()));

        const sockaddr* addr =
            dest.has_value() ? reinterpret_cast<const sockaddr*>(&dest.value()) : nullptr;

        int err = uv_udp_send(&req, &self->handle, &buf, 1, addr, on_send);
        if(err != 0) {
            result = error(err);
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

udp::udp() noexcept : self(nullptr, nullptr) {}

udp::udp(Self* state) noexcept : self(state, Self::destroy) {}

udp::~udp() = default;

udp::udp(udp&& other) noexcept = default;

udp& udp::operator=(udp&& other) noexcept = default;

udp::Self* udp::operator->() noexcept {
    return self.get();
}

const udp::Self* udp::operator->() const noexcept {
    return self.get();
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

result<udp> udp::create(event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    state->buffer.resize(64 * 1024);
    int err = uv_udp_init(static_cast<uv_loop_t*>(loop.handle()), &state->handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->mark_initialized();
    state->handle.data = state.get();
    return udp(state.release());
}

result<udp> udp::create(create_flags flags, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    state->buffer.resize(64 * 1024);
    auto uv_flags = to_uv_udp_init_flags(flags);
    if(!uv_flags.has_value()) {
        return std::unexpected(uv_flags.error());
    }

    int err =
        uv_udp_init_ex(static_cast<uv_loop_t*>(loop.handle()), &state->handle, uv_flags.value());
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->mark_initialized();
    state->handle.data = state.get();
    return udp(state.release());
}

result<udp> udp::open(int fd, event_loop& loop) {
    std::unique_ptr<Self, void (*)(void*)> state(new Self(), Self::destroy);
    state->buffer.resize(64 * 1024);
    int err = uv_udp_init(static_cast<uv_loop_t*>(loop.handle()), &state->handle);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    state->mark_initialized();
    state->handle.data = state.get();

    err = uv_udp_open(&state->handle, fd);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return udp(state.release());
}

error udp::bind(std::string_view host, int port, bind_flags flags) {
    if(!self) {
        return error::invalid_argument;
    }

    auto uv_flags = to_uv_udp_bind_flags(flags);
    if(!uv_flags.has_value()) {
        return uv_flags.error();
    }

    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        return error::invalid_argument;
    }

    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&storage);
    int err = uv_udp_bind(&self->handle, addr, uv_flags.value());
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::connect(std::string_view host, int port) {
    if(!self) {
        return error::invalid_argument;
    }

    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        return error::invalid_argument;
    }

    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&storage);
    int err = uv_udp_connect(&self->handle, addr);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::disconnect() {
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_connect(&self->handle, nullptr);
    if(err != 0) {
        return error(err);
    }

    return {};
}

task<error> udp::send(std::span<const char> data, std::string_view host, int port) {
    if(!self) {
        co_return error::invalid_argument;
    }

    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        co_return error::invalid_argument;
    }

    co_return co_await udp_send_await{self.get(), data, std::optional<sockaddr_storage>(storage)};
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

    sockaddr_storage storage{};
    int family = fill_addr(host, port, storage);
    if(family == AF_UNSPEC) {
        return error::invalid_argument;
    }

    uv_buf_t buf =
        uv_buf_init(const_cast<char*>(data.data()), static_cast<unsigned int>(data.size()));
    int err = uv_udp_try_send(&self->handle, &buf, 1, reinterpret_cast<const sockaddr*>(&storage));
    if(err < 0) {
        return error(err);
    }

    return {};
}

error udp::try_send(std::span<const char> data) {
    if(!self) {
        return error::invalid_argument;
    }

    uv_buf_t buf =
        uv_buf_init(const_cast<char*>(data.data()), static_cast<unsigned int>(data.size()));
    int err = uv_udp_try_send(&self->handle, &buf, 1, nullptr);
    if(err < 0) {
        return error(err);
    }

    return {};
}

error udp::stop_recv() {
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_recv_stop(&self->handle);
    if(err != 0) {
        return error(err);
    }

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
    int err = uv_udp_getsockname(&self->handle, reinterpret_cast<sockaddr*>(&storage), &len);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&storage));
}

result<udp::endpoint> udp::getpeername() const {
    if(!self) {
        return std::unexpected(error::invalid_argument);
    }

    sockaddr_storage storage{};
    int len = sizeof(storage);
    int err = uv_udp_getpeername(&self->handle, reinterpret_cast<sockaddr*>(&storage), &len);
    if(err != 0) {
        return std::unexpected(error(err));
    }

    return endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&storage));
}

error udp::set_membership(std::string_view multicast_addr,
                          std::string_view interface_addr,
                          membership m) {
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_set_membership(&self->handle,
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
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_set_source_membership(&self->handle,
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
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_set_multicast_loop(&self->handle, on ? 1 : 0);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_multicast_ttl(int ttl) {
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_set_multicast_ttl(&self->handle, ttl);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_multicast_interface(std::string_view interface_addr) {
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_set_multicast_interface(&self->handle, std::string(interface_addr).c_str());
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_broadcast(bool on) {
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_set_broadcast(&self->handle, on ? 1 : 0);
    if(err != 0) {
        return error(err);
    }

    return {};
}

error udp::set_ttl(int ttl) {
    if(!self) {
        return error::invalid_argument;
    }

    int err = uv_udp_set_ttl(&self->handle, ttl);
    if(err != 0) {
        return error(err);
    }

    return {};
}

bool udp::using_recvmmsg() const {
    if(!self) {
        return false;
    }

    return uv_udp_using_recvmmsg(&self->handle) != 0;
}

std::size_t udp::send_queue_size() const {
    if(!self) {
        return 0;
    }

    return uv_udp_get_send_queue_size(&self->handle);
}

std::size_t udp::send_queue_count() const {
    if(!self) {
        return 0;
    }

    return uv_udp_get_send_queue_count(&self->handle);
}

}  // namespace eventide
