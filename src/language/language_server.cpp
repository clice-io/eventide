#include "language/language_server.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "eventide/error.h"
#include "language/lsp_methods.inc"

#if __has_include(<yyjson.h>)
#include <yyjson.h>
#else
#include <rfl/thirdparty/yyjson.h>
#endif

#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>

#include "simdjson/simdjson.h"

namespace eventide::language {

namespace {

constexpr std::int32_t kMethodNotFound = -32601;
constexpr std::int32_t kInvalidParams = -32602;

using request_id_t = std::variant<std::int64_t, std::string>;

struct doc_deleter {
    void operator()(yyjson_doc* doc) const noexcept {
        yyjson_doc_free(doc);
    }
};

struct message {
    std::string method;
    std::optional<request_id_t> request_id;
    yyjson_val* params = nullptr;
    std::unique_ptr<yyjson_doc, doc_deleter> doc;
};

struct error_object {
    std::int32_t code = 0;
    std::string message;
    std::optional<rfl::Generic> data;
};

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

std::optional<message> parse_message(std::string_view payload) {
    yyjson_read_err err;
    yyjson_doc* raw =
        yyjson_read_opts(const_cast<char*>(payload.data()), payload.size(), 0, nullptr, &err);
    if(!raw) {
        return std::nullopt;
    }

    std::unique_ptr<yyjson_doc, doc_deleter> doc(raw);
    yyjson_val* root = yyjson_doc_get_root(raw);
    if(!root || !yyjson_is_obj(root)) {
        return std::nullopt;
    }

    yyjson_val* method = yyjson_obj_get(root, "method");
    if(!method || !yyjson_is_str(method)) {
        return std::nullopt;
    }

    message out{};
    out.method.assign(yyjson_get_str(method), yyjson_get_len(method));

    yyjson_val* id_val = yyjson_obj_get(root, "id");
    if(id_val && !yyjson_is_null(id_val)) {
        if(yyjson_is_int(id_val)) {
            out.request_id = request_id_t{static_cast<std::int64_t>(yyjson_get_sint(id_val))};
        } else if(yyjson_is_str(id_val)) {
            out.request_id =
                request_id_t{std::string(yyjson_get_str(id_val), yyjson_get_len(id_val))};
        } else {
            return std::nullopt;
        }
    }

    out.params = yyjson_obj_get(root, "params");
    out.doc = std::move(doc);
    return out;
}

template <typename Params>
result<Params> parse_params(const message& msg) {
    if(msg.params == nullptr || yyjson_is_null(msg.params)) {
        if constexpr(std::is_default_constructible_v<Params>) {
            return Params{};
        } else {
            return std::unexpected(error::invalid_argument);
        }
    }

    auto res = rfl::json::read<Params>(rfl::json::InputVarType(msg.params));
    if(!res) {
        return std::unexpected(error::invalid_argument);
    }

    return std::move(res.value());
}

template <typename Result>
std::string make_result(const request_id_t& request_id, const Result& value) {
    struct response_message {
        std::string jsonrpc = "2.0";
        request_id_t id;
        Result result;
    } response{std::string("2.0"), request_id, value};

    return rfl::json::write(response);
}

std::string make_error(const request_id_t& request_id, const error_object& error) {
    struct error_response {
        std::string jsonrpc = "2.0";
        request_id_t id;
        error_object error;
    } response{std::string("2.0"), request_id, error};

    return rfl::json::write(response);
}

template <typename Params, typename Handler>
task<bool> dispatch_notification_impl(message& msg,
                                      std::string_view method_name,
                                      Handler& handler) {
    if(msg.method != method_name) {
        co_return false;
    }
    if(!handler) {
        co_return true;
    }
    auto params_res = parse_params<Params>(msg);
    if(!params_res.has_value()) {
        co_return true;
    }
    co_await handler(std::move(*params_res));
    co_return true;
}

template <typename Params, typename Result, typename Handler, typename SendFn, typename ScheduleFn>
bool dispatch_request_impl(message& msg,
                           std::string_view method_name,
                           Handler& handler,
                           SendFn&& send_fn,
                           ScheduleFn&& schedule_fn) {
    if(msg.method != method_name) {
        return false;
    }
    if(!handler) {
        error_object err{kMethodNotFound, "Method not found", std::nullopt};
        schedule_fn(send_fn(make_error(*msg.request_id, err)));
        return true;
    }
    auto params_res = parse_params<Params>(msg);
    if(!params_res.has_value()) {
        error_object err{kInvalidParams, "Invalid params", std::nullopt};
        schedule_fn(send_fn(make_error(*msg.request_id, err)));
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
        co_await send(make_result(request_id, result));
    };
    schedule_fn(request_task());
    return true;
}

}  // namespace

struct language_server::Self {
    explicit Self(stream io, event_loop& loop, language_server* owner) :
        io_(std::move(io)), loop_(loop), owner_(owner) {}

    task<> run() {
        while(true) {
            auto payload = co_await read_message();
            if(!payload.has_value()) {
                co_return;
            }

            auto msg = parse_message(*payload);
            if(!msg.has_value()) {
                continue;
            }

            enqueue_message(std::move(msg.value()));
        }
    }

private:
    static std::optional<std::size_t> parse_content_length(std::string_view header) {
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

    task<std::optional<std::string>> read_message() {
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

    task<> send_payload(std::string payload) {
        std::string header = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n";
        std::string out = header + payload;

        co_await write_mutex_.lock();
        co_await io_.write(std::span<const char>(out.data(), out.size()));
        write_mutex_.unlock();
    }

    void enqueue_message(message msg) {
        inbox_.push_back(std::move(msg));
        if(dispatching_) {
            return;
        }
        dispatching_ = true;
        loop_.schedule(dispatch_loop());
    }

    task<> dispatch_loop() {
        while(!inbox_.empty()) {
            auto msg = std::move(inbox_.front());
            inbox_.pop_front();

            if(!msg.request_id.has_value()) {
                co_await dispatch_notification(msg);
                continue;
            }

            if(!dispatch_request(msg)) {
                error_object err{kMethodNotFound, "Method not found", std::nullopt};
                co_await send_payload(make_error(*msg.request_id, err));
            }
        }

        dispatching_ = false;
    }

    task<> dispatch_notification(message& msg) {
#define EVENTIDE_LSP_UNWRAP(...) __VA_ARGS__

#define EVENTIDE_LSP_DISPATCH_NOTIFICATION(name, method_name, Params)                              \
    if(co_await dispatch_notification_impl<EVENTIDE_LSP_UNWRAP Params>(msg,                        \
                                                                       method_name,                \
                                                                       owner_->name##_handler_)) { \
        co_return;                                                                                 \
    }

        EVENTIDE_LSP_NOTIFICATIONS(EVENTIDE_LSP_DISPATCH_NOTIFICATION)
#undef EVENTIDE_LSP_DISPATCH_NOTIFICATION
#undef EVENTIDE_LSP_UNWRAP

        co_return;
    }

    bool dispatch_request(message& msg) {
#define EVENTIDE_LSP_UNWRAP(...) __VA_ARGS__

#define EVENTIDE_LSP_DISPATCH_REQUEST(name, method_name, Params, Result)                           \
    if(dispatch_request_impl<EVENTIDE_LSP_UNWRAP Params, EVENTIDE_LSP_UNWRAP Result>(              \
           msg,                                                                                    \
           method_name,                                                                            \
           owner_->name##_handler_,                                                                \
           [this](std::string payload) { return send_payload(std::move(payload)); },               \
           [this](auto&& task) { loop_.schedule(std::forward<decltype(task)>(task)); })) {         \
        return true;                                                                               \
    }

        EVENTIDE_LSP_REQUESTS(EVENTIDE_LSP_DISPATCH_REQUEST)
#undef EVENTIDE_LSP_DISPATCH_REQUEST
#undef EVENTIDE_LSP_UNWRAP

        return false;
    }

    stream io_{};
    event_loop& loop_;
    mutex write_mutex_{};
    std::deque<message> inbox_{};
    bool dispatching_ = false;
    language_server* owner_ = nullptr;
};

language_server::language_server(stream io, event_loop& loop) :
    self_(std::make_unique<Self>(std::move(io), loop, this)) {}

language_server::~language_server() = default;

task<> language_server::run() {
    return self_->run();
}

}  // namespace eventide::language
