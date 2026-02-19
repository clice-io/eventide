#pragma once

#include <expected>
#include <functional>

#include "protocol.h"
#include "eventide/task.h"

namespace language {

struct RequestContext {};

template <typename Params>
using RequestResult = std::expected<typename protocol::RequestTraits<Params>::Result, std::string>;

class LanguageServer {
public:
    LanguageServer();

    LanguageServer(const LanguageServer&) = delete;

    LanguageServer(LanguageServer&&) = delete;

    ~LanguageServer();

public:
    int start();

    template <typename Params, typename Result = protocol::RequestTraits<Params>::Result>
    using callback =
        std::function<eventide::task<std::expected<Result, std::string>>(RequestContext& request,
                                                                         const Params& params)>;

    template <typename Params>
    void on_request(callback<Params> callback) {
        std::string_view method = protocol::RequestTraits<Params>::method;
    }

    template <typename Params>
    void on_notification();

private:
    struct Self;
    std::unique_ptr<Self> self;
};

using namespace eventide;
using namespace protocol;

void foo() {
    LanguageServer server;
    server.on_request<InitializeParams>(
        [](RequestContext& request,
           const InitializeParams& params) -> task<std::expected<InitializeResult, std::string>> {
            co_return std::unexpected(std::string());
        });
}

}  // namespace language
