#pragma once

#include <expected>
#include <memory>
#include <string_view>

#include "eventide/jsonrpc/peer.h"
#include "eventide/async/task.h"
#include "eventide/language/protocol.h"
#include "eventide/language/transport.h"

namespace eventide::language {

class LanguageServer;

template <typename Params, typename Result = typename protocol::RequestTraits<Params>::Result>
using RequestResult = task<std::expected<Result, std::string>>;

using RequestContext = jsonrpc::basic_request_context<LanguageServer>;

class LanguageServer : public jsonrpc::Peer {
public:
    LanguageServer();

    explicit LanguageServer(std::unique_ptr<Transport> transport);

    ~LanguageServer();

    using jsonrpc::Peer::start;

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
};

}  // namespace eventide::language

#define EVENTIDE_LANGUAGE_SERVER_INL_FROM_HEADER
#include "eventide/language/server.inl"
#undef EVENTIDE_LANGUAGE_SERVER_INL_FROM_HEADER
