#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "eventide/loop.h"
#include "eventide/stream.h"
#include "eventide/sync.h"
#include "eventide/task.h"
#include "language/json_rpc.h"
#include "language/lsp_types.h"

namespace eventide::language {

using namespace proto;

class language_server {
public:
    explicit language_server(stream io, event_loop& loop = event_loop::current());

#define EVENTIDE_LSP_UNWRAP(...) __VA_ARGS__

#define EVENTIDE_LSP_DECLARE_REQUEST(name, method, Params, Result)                                 \
    using name##_handler_t =                                                                       \
        std::move_only_function<task<EVENTIDE_LSP_UNWRAP Result>(EVENTIDE_LSP_UNWRAP Params)>;     \
    void on_##name(name##_handler_t handler) {                                                     \
        name##_handler_ = std::move(handler);                                                      \
    }                                                                                              \
    name##_handler_t name##_handler_{};

#define EVENTIDE_LSP_DECLARE_NOTIFICATION(name, method, Params)                                    \
    using name##_handler_t = std::move_only_function<task<>(EVENTIDE_LSP_UNWRAP Params)>;          \
    void on_##name(name##_handler_t handler) {                                                     \
        name##_handler_ = std::move(handler);                                                      \
    }                                                                                              \
    name##_handler_t name##_handler_{};

#include "language/lsp_methods.inc"

    EVENTIDE_LSP_REQUESTS(EVENTIDE_LSP_DECLARE_REQUEST)
    EVENTIDE_LSP_NOTIFICATIONS(EVENTIDE_LSP_DECLARE_NOTIFICATION)

#undef EVENTIDE_LSP_DECLARE_REQUEST
#undef EVENTIDE_LSP_DECLARE_NOTIFICATION
#undef EVENTIDE_LSP_UNWRAP

    task<> run();

private:
    static std::optional<std::size_t> parse_content_length(std::string_view header);

    task<std::optional<std::string>> read_message();
    task<> send_payload(std::string payload);

    void enqueue_message(json_rpc::message msg);
    task<> dispatch_loop();
    task<> dispatch_notification(json_rpc::message& msg);
    bool dispatch_request(json_rpc::message& msg);

    stream io_{};
    event_loop& loop_;
    mutex write_mutex_{};
    std::deque<json_rpc::message> inbox_{};
    bool dispatching_ = false;
};

}  // namespace eventide::language
