#include "eventide/language/server.h"

#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "eventide/async/sync.h"

namespace eventide::language {

namespace {

template <typename T>
std::expected<T, std::string> parse_json_value(std::string_view json) {
    auto parsed = serde::json::simd::from_json<T>(json);
    if(!parsed) {
        return std::unexpected(std::string(simdjson::error_message(parsed.error())));
    }
    return std::move(*parsed);
}

std::expected<protocol::RequestID, std::string> parse_request_id(std::string_view id_json) {
    auto id_integer = parse_json_value<protocol::integer>(id_json);
    if(id_integer) {
        return protocol::RequestID{*id_integer};
    }

    auto id_string = parse_json_value<protocol::string>(id_json);
    if(id_string) {
        return protocol::RequestID{std::move(*id_string)};
    }

    return std::unexpected("request id must be integer or string");
}

std::expected<protocol::IncomingMessage, std::string>
    parse_incoming_message(std::string_view payload) {
    simdjson::ondemand::parser parser;
    simdjson::padded_string json(payload);

    simdjson::ondemand::document document{};
    auto document_result = parser.iterate(json);
    auto document_error = std::move(document_result).get(document);
    if(document_error != simdjson::SUCCESS) {
        return std::unexpected(std::string(simdjson::error_message(document_error)));
    }

    simdjson::ondemand::object object{};
    auto object_result = document.get_object();
    auto object_error = std::move(object_result).get(object);
    if(object_error != simdjson::SUCCESS) {
        return std::unexpected(std::string(simdjson::error_message(object_error)));
    }

    protocol::IncomingMessage message{};
    for(auto field_result: object) {
        simdjson::ondemand::field field{};
        auto field_error = std::move(field_result).get(field);
        if(field_error != simdjson::SUCCESS) {
            return std::unexpected(std::string(simdjson::error_message(field_error)));
        }

        std::string_view key;
        auto key_error = field.unescaped_key().get(key);
        if(key_error != simdjson::SUCCESS) {
            return std::unexpected(std::string(simdjson::error_message(key_error)));
        }

        auto value = field.value();
        std::string_view raw_json;
        auto raw_error = value.raw_json().get(raw_json);
        if(raw_error != simdjson::SUCCESS) {
            return std::unexpected(std::string(simdjson::error_message(raw_error)));
        }

        if(key == "method") {
            auto method = parse_json_value<std::string>(raw_json);
            if(!method) {
                return std::unexpected(method.error());
            }
            message.method = std::move(*method);
            continue;
        }

        if(key == "id") {
            auto id = parse_request_id(raw_json);
            if(!id) {
                return std::unexpected(id.error());
            }
            message.id = std::move(*id);
            continue;
        }

        if(key == "params") {
            message.params_json = std::string(raw_json);
            continue;
        }

        if(key == "result") {
            message.result_json = std::string(raw_json);
            continue;
        }

        if(key == "error") {
            message.error_json = std::string(raw_json);
            continue;
        }
    }

    if(!document.at_end()) {
        return std::unexpected("trailing content after JSON-RPC message");
    }

    return message;
}

struct request_id_hash {
    std::size_t operator()(const protocol::RequestID& id) const noexcept {
        return std::visit(
            [](const auto& value) -> std::size_t {
                using value_t = std::remove_cvref_t<decltype(value)>;
                auto hashed = std::hash<value_t>{}(value);
                if constexpr(std::is_same_v<value_t, protocol::integer>) {
                    return hashed ^ 0x9e3779b97f4a7c15ULL;
                } else {
                    return hashed ^ 0x517cc1b727220a95ULL;
                }
            },
            id);
    }
};

struct outgoing_request_message {
    std::string jsonrpc = "2.0";
    protocol::RequestID id;
    std::string method;
    protocol::LSPAny params;
};

struct outgoing_notification_message {
    std::string jsonrpc = "2.0";
    std::string method;
    protocol::LSPAny params;
};

struct outgoing_success_response_message {
    std::string jsonrpc = "2.0";
    protocol::RequestID id;
    protocol::LSPAny result;
};

struct outgoing_error_response_message {
    std::string jsonrpc = "2.0";
    protocol::RequestID id;
    protocol::ResponseError error;
};

}  // namespace

struct LanguageServer::Self {
    struct PendingRequest {
        event ready;
        std::optional<std::expected<std::string, std::string>> response;
    };

    event_loop loop;
    std::unique_ptr<Transport> transport;
    std::unordered_map<std::string, RequestCallback> request_callbacks;
    std::unordered_map<std::string, NotificationCallback> notification_callbacks;
    std::unordered_map<protocol::RequestID, std::shared_ptr<PendingRequest>, request_id_hash>
        pending_requests;
    protocol::integer next_request_id = 1;
    std::deque<std::string> outgoing_queue;
    bool writer_running = false;
    std::string startup_error;

    void enqueue_outgoing(std::string payload) {
        outgoing_queue.push_back(std::move(payload));
        if(!writer_running) {
            writer_running = true;
            loop.schedule(write_loop());
        }
    }

    std::expected<std::string, std::string> build_success_response(const protocol::RequestID& id,
                                                                   std::string_view result_json) {
        auto result = parse_json_value<protocol::LSPAny>(result_json);
        if(!result) {
            return std::unexpected(result.error());
        }

        return detail::serialize_json(outgoing_success_response_message{
            .id = id,
            .result = std::move(*result),
        });
    }

    std::expected<std::string, std::string> build_error_response(const protocol::RequestID& id,
                                                                 protocol::integer code,
                                                                 std::string message) {
        return detail::serialize_json(outgoing_error_response_message{
            .id = id,
            .error =
                protocol::ResponseError{
                                        .code = code,
                                        .message = std::move(message),
                                        },
        });
    }

    void send_error(const protocol::RequestID& id, protocol::integer code, std::string message) {
        auto response = build_error_response(id, code, std::move(message));
        if(response) {
            enqueue_outgoing(std::move(*response));
        }
    }

    void complete_pending_request(const protocol::RequestID& id,
                                  std::expected<std::string, std::string> response) {
        auto it = pending_requests.find(id);
        if(it == pending_requests.end()) {
            return;
        }

        auto pending = std::move(it->second);
        pending_requests.erase(it);
        pending->response = std::move(response);
        pending->ready.set();
    }

    void fail_pending_requests(const std::string& message) {
        if(pending_requests.empty()) {
            return;
        }

        std::vector<std::shared_ptr<PendingRequest>> pending;
        pending.reserve(pending_requests.size());
        for(auto& [_, state]: pending_requests) {
            pending.push_back(state);
        }
        pending_requests.clear();

        for(auto& state: pending) {
            state->response = std::unexpected(message);
            state->ready.set();
        }
    }

    task<> write_loop() {
        while(!outgoing_queue.empty()) {
            auto payload = std::move(outgoing_queue.front());
            outgoing_queue.pop_front();

            if(!transport) {
                break;
            }

            auto written = co_await transport->write_message(payload);
            if(!written) {
                outgoing_queue.clear();
                fail_pending_requests("transport write failed");
                break;
            }
        }

        writer_running = false;
    }

    void dispatch_notification(const std::string& method, std::string_view params_json) {
        auto it = notification_callbacks.find(method);
        if(it == notification_callbacks.end()) {
            return;
        }

        // Notification callbacks are synchronous.
        it->second(params_json);
    }

    task<> run_request(protocol::RequestID id, RequestCallback callback, std::string params_json) {
        auto result_json = co_await callback(id, params_json);
        if(!result_json) {
            send_error(id,
                       static_cast<protocol::integer>(protocol::LSPErrorCodes::RequestFailed),
                       result_json.error());
            co_return;
        }

        auto response = build_success_response(id, *result_json);
        if(!response) {
            send_error(id,
                       static_cast<protocol::integer>(protocol::ErrorCodes::InternalError),
                       response.error());
            co_return;
        }

        enqueue_outgoing(std::move(*response));
    }

    void dispatch_request(const std::string& method,
                          const protocol::RequestID& id,
                          std::string_view params_json) {
        auto it = request_callbacks.find(method);
        if(it == request_callbacks.end()) {
            send_error(id,
                       static_cast<protocol::integer>(protocol::ErrorCodes::MethodNotFound),
                       "method not found: " + method);
            return;
        }

        auto callback = it->second;
        // Requests are processed asynchronously.
        loop.schedule(run_request(id, std::move(callback), std::string(params_json)));
    }

    void dispatch_response(const protocol::RequestID& id,
                           const protocol::optional<std::string>& result_json,
                           const protocol::optional<std::string>& error_json) {
        if(error_json.has_value()) {
            auto parsed_error = parse_json_value<protocol::ResponseError>(*error_json);
            if(!parsed_error) {
                complete_pending_request(id, std::unexpected(parsed_error.error()));
                return;
            }

            complete_pending_request(id, std::unexpected(parsed_error->message));
            return;
        }

        if(!result_json.has_value()) {
            complete_pending_request(id, std::unexpected("response is missing result"));
            return;
        }

        complete_pending_request(id, std::string(*result_json));
    }

    task<> main_loop() {
        while(transport) {
            auto payload = co_await transport->read_message();
            if(!payload.has_value()) {
                fail_pending_requests("transport closed");
                co_return;
            }

            auto parsed = parse_incoming_message(*payload);
            if(!parsed) {
                continue;
            }

            if(parsed->method.has_value()) {
                std::string_view params_json{};
                if(parsed->params_json.has_value()) {
                    params_json = *parsed->params_json;
                }

                if(parsed->id.has_value()) {
                    dispatch_request(*parsed->method, *parsed->id, params_json);
                } else {
                    dispatch_notification(*parsed->method, params_json);
                }
                continue;
            }

            if(parsed->id.has_value()) {
                dispatch_response(*parsed->id, parsed->result_json, parsed->error_json);
            }
        }
    }
};

LanguageServer::LanguageServer() : self(std::make_unique<Self>()) {
    auto stdio = StreamTransport::open_stdio(self->loop);
    if(!stdio) {
        self->startup_error = stdio.error();
        return;
    }
    self->transport = std::move(*stdio);
}

LanguageServer::LanguageServer(std::unique_ptr<Transport> transport) :
    self(std::make_unique<Self>()) {
    self->transport = std::move(transport);
    if(!self->transport) {
        self->startup_error = "transport is null";
    }
}

LanguageServer::~LanguageServer() = default;

void LanguageServer::register_request_callback(std::string_view method, RequestCallback callback) {
    self->request_callbacks.insert_or_assign(std::string(method), std::move(callback));
}

void LanguageServer::register_notification_callback(std::string_view method,
                                                    NotificationCallback callback) {
    self->notification_callbacks.insert_or_assign(std::string(method), std::move(callback));
}

task<std::expected<std::string, std::string>>
    LanguageServer::send_request_json(std::string_view method, std::string params_json) {
    if(!self || !self->transport) {
        co_return std::unexpected("transport is null");
    }

    auto params = parse_json_value<protocol::LSPAny>(params_json);
    if(!params) {
        co_return std::unexpected(params.error());
    }

    auto request_id = protocol::RequestID{self->next_request_id++};

    auto pending = std::make_shared<Self::PendingRequest>();
    self->pending_requests.insert_or_assign(request_id, pending);

    auto request_json = detail::serialize_json(outgoing_request_message{
        .id = request_id,
        .method = std::string(method),
        .params = std::move(*params),
    });
    if(!request_json) {
        self->pending_requests.erase(request_id);
        co_return std::unexpected(request_json.error());
    }

    self->enqueue_outgoing(std::move(*request_json));

    co_await pending->ready.wait();
    if(!pending->response.has_value()) {
        co_return std::unexpected("request was not completed");
    }

    co_return std::move(*pending->response);
}

std::expected<void, std::string> LanguageServer::send_notification_json(std::string_view method,
                                                                        std::string params_json) {
    if(!self || !self->transport) {
        return std::unexpected("transport is null");
    }

    auto params = parse_json_value<protocol::LSPAny>(params_json);
    if(!params) {
        return std::unexpected(params.error());
    }

    auto notification_json = detail::serialize_json(outgoing_notification_message{
        .method = std::string(method),
        .params = std::move(*params),
    });
    if(!notification_json) {
        return std::unexpected(notification_json.error());
    }

    self->enqueue_outgoing(std::move(*notification_json));
    return {};
}

int LanguageServer::start() {
    if(!self || !self->transport) {
        return -1;
    }

    self->loop.schedule(self->main_loop());
    return self->loop.run();
}

}  // namespace eventide::language
