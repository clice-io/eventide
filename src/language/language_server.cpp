#include "language/language_server.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "language/lsp_methods.inc"

namespace eventide::language {

namespace {

constexpr std::int32_t kMethodNotFound = -32601;
constexpr std::int32_t kInvalidParams = -32602;

std::string_view trim(std::string_view value) {
    auto start = value.find_first_not_of(" \t");
    if(start == std::string_view::npos) {
        return {};
    }
    auto end = value.find_last_not_of(" \t");
    return value.substr(start, end - start + 1);
}

bool iequals(std::string_view lhs, std::string_view rhs) {
    if(lhs.size() != rhs.size()) {
        return false;
    }
    for(std::size_t i = 0; i < lhs.size(); ++i) {
        if(std::tolower(static_cast<unsigned char>(lhs[i])) !=
           std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

template <typename Params, typename Handler>
task<bool> dispatch_notification_impl(json_rpc::message& msg,
                                      std::string_view method_name,
                                      Handler& handler) {
    if(msg.method != method_name) {
        co_return false;
    }
    if(!handler) {
        co_return true;
    }
    auto params_res = json_rpc::parse_params<Params>(msg);
    if(!params_res.has_value()) {
        co_return true;
    }
    co_await handler(std::move(*params_res));
    co_return true;
}

template <typename Params, typename Result, typename Handler, typename SendFn, typename ScheduleFn>
bool dispatch_request_impl(json_rpc::message& msg,
                           std::string_view method_name,
                           Handler& handler,
                           SendFn&& send_fn,
                           ScheduleFn&& schedule_fn) {
    if(msg.method != method_name) {
        return false;
    }
    if(!handler) {
        json_rpc::error_object err{kMethodNotFound, "Method not found", std::nullopt};
        schedule_fn(send_fn(json_rpc::make_error(*msg.request_id, err)));
        return true;
    }
    auto params_res = json_rpc::parse_params<Params>(msg);
    if(!params_res.has_value()) {
        json_rpc::error_object err{kInvalidParams, "Invalid params", std::nullopt};
        schedule_fn(send_fn(json_rpc::make_error(*msg.request_id, err)));
        return true;
    }

    auto handler_ptr = &handler;
    auto request_id = *msg.request_id;
    auto params = std::move(*params_res);
    auto send = std::forward<SendFn>(send_fn);

    auto request_task = [handler_ptr,
                         request_id,
                         params = std::move(params),
                         send = std::move(send)]() mutable -> task<> {
        auto result = co_await (*handler_ptr)(std::move(params));
        co_await send(json_rpc::make_result(request_id, result));
    };
    schedule_fn(request_task());
    return true;
}

}  // namespace

language_server::language_server(stream io, event_loop& loop) : io_(std::move(io)), loop_(loop) {}

std::optional<std::size_t> language_server::parse_content_length(std::string_view header) {
    std::size_t pos = 0;
    while(pos < header.size()) {
        auto end = header.find("\r\n", pos);
        if(end == std::string_view::npos) {
            break;
        }
        auto line = header.substr(pos, end - pos);
        pos = end + 2;
        auto sep = line.find(':');
        if(sep == std::string_view::npos) {
            continue;
        }
        auto name = trim(line.substr(0, sep));
        if(!iequals(name, "Content-Length")) {
            continue;
        }
        auto value = trim(line.substr(sep + 1));
        if(value.empty()) {
            return std::nullopt;
        }
        std::size_t length = 0;
        for(char ch: value) {
            if(ch < '0' || ch > '9') {
                return std::nullopt;
            }
            length = length * 10 + static_cast<std::size_t>(ch - '0');
        }
        return length;
    }
    return std::nullopt;
}

task<std::optional<std::string>> language_server::read_message() {
    std::string header;
    std::optional<std::size_t> content_length;

    while(!content_length.has_value()) {
        auto chunk = co_await io_.read_chunk();
        if(chunk.empty()) {
            co_return std::nullopt;
        }

        auto prior = header.size();
        header.append(chunk.data(), chunk.size());

        auto marker = header.find("\r\n\r\n");
        if(marker == std::string::npos) {
            io_.consume(chunk.size());
            continue;
        }

        auto header_end = marker + 4;
        auto before_chunk = prior;
        auto header_from_chunk = header_end > before_chunk ? header_end - before_chunk : 0;
        io_.consume(header_from_chunk);

        auto header_view = std::string_view(header.data(), header_end);
        content_length = parse_content_length(header_view);
        if(!content_length.has_value()) {
            co_return std::nullopt;
        }
        break;
    }

    std::string payload;
    payload.reserve(*content_length);

    while(payload.size() < *content_length) {
        auto chunk = co_await io_.read_chunk();
        if(chunk.empty()) {
            co_return std::nullopt;
        }

        auto need = *content_length - payload.size();
        auto take = std::min<std::size_t>(need, chunk.size());
        payload.append(chunk.data(), take);
        io_.consume(take);
    }

    co_return payload;
}

task<> language_server::send_payload(std::string payload) {
    std::string header = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n";
    std::string out = header + payload;

    co_await write_mutex_.lock();
    co_await io_.write(std::span<const char>(out.data(), out.size()));
    write_mutex_.unlock();
}

void language_server::enqueue_message(json_rpc::message msg) {
    inbox_.push_back(std::move(msg));
    if(dispatching_) {
        return;
    }
    dispatching_ = true;
    loop_.schedule(dispatch_loop());
}

task<> language_server::dispatch_loop() {
    while(!inbox_.empty()) {
        auto msg = std::move(inbox_.front());
        inbox_.pop_front();

        if(!msg.request_id.has_value()) {
            co_await dispatch_notification(msg);
            continue;
        }

        if(!dispatch_request(msg)) {
            json_rpc::error_object err{kMethodNotFound, "Method not found", std::nullopt};
            co_await send_payload(json_rpc::make_error(*msg.request_id, err));
        }
    }

    dispatching_ = false;
}

task<> language_server::dispatch_notification(json_rpc::message& msg) {
#define EVENTIDE_LSP_UNWRAP(...) __VA_ARGS__

#define EVENTIDE_LSP_DISPATCH_NOTIFICATION(name, method_name, Params)                              \
    if(co_await dispatch_notification_impl<EVENTIDE_LSP_UNWRAP Params>(msg,                        \
                                                                       method_name,                \
                                                                       name##_handler_)) {         \
        co_return;                                                                                 \
    }

    EVENTIDE_LSP_NOTIFICATIONS(EVENTIDE_LSP_DISPATCH_NOTIFICATION)
#undef EVENTIDE_LSP_DISPATCH_NOTIFICATION
#undef EVENTIDE_LSP_UNWRAP

    co_return;
}

bool language_server::dispatch_request(json_rpc::message& msg) {
#define EVENTIDE_LSP_UNWRAP(...) __VA_ARGS__

#define EVENTIDE_LSP_DISPATCH_REQUEST(name, method_name, Params, Result)                           \
    if(dispatch_request_impl<EVENTIDE_LSP_UNWRAP Params, EVENTIDE_LSP_UNWRAP Result>(              \
           msg,                                                                                    \
           method_name,                                                                            \
           name##_handler_,                                                                        \
           [this](std::string payload) { return send_payload(std::move(payload)); },               \
           [this](auto&& task) { loop_.schedule(std::forward<decltype(task)>(task)); })) {         \
        return true;                                                                               \
    }

    EVENTIDE_LSP_REQUESTS(EVENTIDE_LSP_DISPATCH_REQUEST)
#undef EVENTIDE_LSP_DISPATCH_REQUEST
#undef EVENTIDE_LSP_UNWRAP

    return false;
}

task<> language_server::run() {
    while(true) {
        auto payload = co_await read_message();
        if(!payload.has_value()) {
            co_return;
        }

        auto msg = json_rpc::parse(*payload);
        if(!msg.has_value()) {
            continue;
        }

        enqueue_message(std::move(*msg));
    }
}

}  // namespace eventide::language
