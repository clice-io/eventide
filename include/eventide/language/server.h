#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "eventide/async/task.h"
#include "eventide/language/protocol.h"
#include "eventide/language/transport.h"

namespace eventide::language {

class LanguageServer;

template <typename Params, typename Result = typename protocol::RequestTraits<Params>::Result>
using RequestResult = task<std::expected<Result, std::string>>;

struct RequestContext {
    std::string_view method{};
    protocol::RequestID id;
    LanguageServer& server;

    RequestContext(LanguageServer& server, const protocol::RequestID& id) :
        id(id), server(server) {}

    LanguageServer* operator->() noexcept {
        return &server;
    }

    const LanguageServer* operator->() const noexcept {
        return &server;
    }
};

class LanguageServer {
public:
    LanguageServer();

    explicit LanguageServer(std::unique_ptr<Transport> transport);

    LanguageServer(const LanguageServer&) = delete;
    LanguageServer& operator=(const LanguageServer&) = delete;
    LanguageServer(LanguageServer&&) = delete;
    LanguageServer& operator=(LanguageServer&&) = delete;

    ~LanguageServer();

    int start();

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

}  // namespace eventide::language

#define EVENTIDE_LANGUAGE_SERVER_INL_FROM_HEADER
#include "eventide/language/server.inl"
#undef EVENTIDE_LANGUAGE_SERVER_INL_FROM_HEADER
