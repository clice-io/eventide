#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "eventide/jsonrpc/protocol.h"
#include "eventide/jsonrpc/transport.h"
#include "eventide/async/loop.h"
#include "eventide/async/task.h"

namespace eventide::jsonrpc {

class Peer;

template <typename PeerT>
struct basic_request_context {
    std::string_view method{};
    protocol::RequestID id;
    PeerT& peer;

    basic_request_context(PeerT& peer, const protocol::RequestID& id) : id(id), peer(peer) {}

    PeerT* operator->() noexcept {
        return &peer;
    }

    const PeerT* operator->() const noexcept {
        return &peer;
    }
};

using RequestContext = basic_request_context<Peer>;

template <typename Params, typename Result = typename protocol::RequestTraits<Params>::Result>
using RequestResult = task<std::expected<Result, std::string>>;

class Peer {
public:
    Peer();

    explicit Peer(event_loop& loop);

    explicit Peer(std::unique_ptr<Transport> transport);

    Peer(event_loop& loop, std::unique_ptr<Transport> transport);

    Peer(const Peer&) = delete;
    Peer& operator=(const Peer&) = delete;
    Peer(Peer&&) = delete;
    Peer& operator=(Peer&&) = delete;

    ~Peer();

    int start();

    std::expected<void, std::string> close_output();

    template <typename Params>
    RequestResult<Params> send_request(const Params& params);

    template <typename Result, typename Params>
    task<std::expected<Result, std::string>> send_request(std::string_view method,
                                                          const Params& params);

    template <typename Params>
    std::expected<void, std::string> send_notification(const Params& params);

    template <typename Params>
    std::expected<void, std::string> send_notification(std::string_view method,
                                                       const Params& params);

    template <typename Callback>
    void on_request(Callback&& callback);

    template <typename Callback>
    void on_request(std::string_view method, Callback&& callback);

    template <typename Callback>
    void on_notification(Callback&& callback);

    template <typename Callback>
    void on_notification(std::string_view method, Callback&& callback);

private:
    template <typename Params, typename Callback>
    void bind_request_callback(std::string_view method, Callback&& callback);

    template <typename Params, typename Callback>
    void bind_notification_callback(std::string_view method, Callback&& callback);

    using RequestCallback =
        std::function<task<std::expected<std::string, std::string>>(const protocol::RequestID&,
                                                                    std::string_view)>;
    using NotificationCallback = std::function<void(std::string_view)>;

    void register_request_callback(std::string_view method, RequestCallback callback);

    void register_notification_callback(std::string_view method, NotificationCallback callback);

    task<std::expected<std::string, std::string>> send_request_json(std::string_view method,
                                                                    std::string params_json);

    std::expected<void, std::string> send_notification_json(std::string_view method,
                                                            std::string params_json);

private:
    struct Self;
    std::unique_ptr<Self> self;
};

}  // namespace eventide::jsonrpc

#define EVENTIDE_JSONRPC_PEER_INL_FROM_HEADER
#include "eventide/jsonrpc/peer.inl"
#undef EVENTIDE_JSONRPC_PEER_INL_FROM_HEADER
