#include "language/server.h"

#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace language {

namespace et = eventide;

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
    }

    if(!document.at_end()) {
        return std::unexpected("trailing content after JSON-RPC message");
    }

    return message;
}

}  // namespace

struct LanguageServer::Self {
    et::event_loop loop;
    std::unique_ptr<Transport> transport;
    std::unordered_map<std::string, RequestHandler> request_handlers;
    std::unordered_map<std::string, NotificationHandler> notification_handlers;
    std::deque<std::string> outgoing_queue;
    bool writer_running = false;
    std::string startup_error;

    static std::expected<std::string, std::string>
        serialize_id_json(const protocol::RequestID& id) {
        return std::visit(
            [&](const auto& value) -> std::expected<std::string, std::string> {
                using value_t = std::remove_cvref_t<decltype(value)>;
                if constexpr(std::is_same_v<value_t, protocol::integer>) {
                    return std::to_string(value);
                } else {
                    return LanguageServer::serialize_json(value);
                }
            },
            id);
    }

    void enqueue_outgoing(std::string payload) {
        outgoing_queue.push_back(std::move(payload));
        if(!writer_running) {
            writer_running = true;
            loop.schedule(write_loop());
        }
    }

    std::expected<std::string, std::string> build_success_response(const protocol::RequestID& id,
                                                                   std::string_view result_json) {
        auto id_json = serialize_id_json(id);
        if(!id_json) {
            return std::unexpected(id_json.error());
        }

        std::string response;
        response.reserve(48 + id_json->size() + result_json.size());
        response.append("{\"jsonrpc\":\"2.0\",\"id\":");
        response.append(*id_json);
        response.append(",\"result\":");
        response.append(result_json.data(), result_json.size());
        response.push_back('}');
        return response;
    }

    std::expected<std::string, std::string> build_error_response(const protocol::RequestID& id,
                                                                 protocol::integer code,
                                                                 std::string message) {
        auto id_json = serialize_id_json(id);
        if(!id_json) {
            return std::unexpected(id_json.error());
        }

        auto error_json = LanguageServer::serialize_json(protocol::ResponseError{
            .code = code,
            .message = std::move(message),
        });
        if(!error_json) {
            return std::unexpected(error_json.error());
        }

        std::string response;
        response.reserve(56 + id_json->size() + error_json->size());
        response.append("{\"jsonrpc\":\"2.0\",\"id\":");
        response.append(*id_json);
        response.append(",\"error\":");
        response.append(*error_json);
        response.push_back('}');
        return response;
    }

    void send_error(const protocol::RequestID& id, protocol::integer code, std::string message) {
        auto response = build_error_response(id, code, std::move(message));
        if(response) {
            enqueue_outgoing(std::move(*response));
        }
    }

    et::task<> write_loop() {
        while(!outgoing_queue.empty()) {
            auto payload = std::move(outgoing_queue.front());
            outgoing_queue.pop_front();

            if(!transport) {
                break;
            }

            auto written = co_await transport->write_message(payload);
            if(!written) {
                outgoing_queue.clear();
                break;
            }
        }

        writer_running = false;
    }

    void dispatch_notification(const std::string& method, std::string_view params_json) {
        auto it = notification_handlers.find(method);
        if(it == notification_handlers.end()) {
            return;
        }

        // Notification handlers are synchronous.
        it->second(params_json);
    }

    et::task<> run_request(protocol::RequestID id,
                           RequestHandler handler,
                           std::string params_json) {
        auto result_json = co_await handler(params_json);
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
        auto it = request_handlers.find(method);
        if(it == request_handlers.end()) {
            send_error(id,
                       static_cast<protocol::integer>(protocol::ErrorCodes::MethodNotFound),
                       "method not found: " + method);
            return;
        }

        auto handler = it->second;
        // Requests are processed asynchronously.
        loop.schedule(run_request(id, std::move(handler), std::string(params_json)));
    }

    et::task<> main_loop() {
        while(transport) {
            auto payload = co_await transport->read_message();
            if(!payload.has_value()) {
                co_return;
            }

            auto parsed = parse_incoming_message(*payload);
            if(!parsed) {
                continue;
            }
            if(!parsed->method.has_value()) {
                continue;
            }

            std::string_view params_json{};
            if(parsed->params_json.has_value()) {
                params_json = *parsed->params_json;
            }

            if(parsed->id.has_value()) {
                dispatch_request(*parsed->method, *parsed->id, params_json);
            } else {
                dispatch_notification(*parsed->method, params_json);
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

void LanguageServer::register_request_handler(std::string_view method, RequestHandler handler) {
    self->request_handlers.insert_or_assign(std::string(method), std::move(handler));
}

void LanguageServer::register_notification_handler(std::string_view method,
                                                   NotificationHandler handler) {
    self->notification_handlers.insert_or_assign(std::string(method), std::move(handler));
}

int LanguageServer::start() {
    if(!self || !self->transport) {
        return -1;
    }

    self->loop.schedule(self->main_loop());
    return self->loop.run();
}

}  // namespace language
