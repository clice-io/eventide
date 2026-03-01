#include "eventide/jsonrpc/peer.h"

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

#include "eventide/async/cancellation.h"
#include "eventide/async/sync.h"
#include "eventide/async/watcher.h"

namespace eventide::jsonrpc {

namespace {

template <typename T>
Result<T> parse_json_value(std::string_view json,
                           protocol::ErrorCode code = protocol::ErrorCode::RequestFailed) {
    auto parsed = serde::json::parse<T>(json);
    if(!parsed) {
        return std::unexpected(
            RPCError(code, std::string(serde::json::error_message(parsed.error()))));
    }
    return std::move(*parsed);
}

Result<protocol::RequestID> parse_request_id(simdjson::ondemand::value& value) {
    std::int64_t integer_id = 0;
    auto integer_error = value.get_int64().get(integer_id);
    if(integer_error == simdjson::SUCCESS) {
        if(integer_id < (std::numeric_limits<protocol::integer>::min)() ||
           integer_id > (std::numeric_limits<protocol::integer>::max)()) {
            return std::unexpected(RPCError(protocol::ErrorCode::InvalidRequest,
                                            "request id integer is out of range"));
        }
        return protocol::RequestID{static_cast<protocol::integer>(integer_id)};
    }

    std::string_view string_id;
    auto string_error = value.get_string().get(string_id);
    if(string_error == simdjson::SUCCESS) {
        return protocol::RequestID{std::string(string_id)};
    }

    return std::unexpected(
        RPCError(protocol::ErrorCode::InvalidRequest, "request id must be integer or string"));
}

protocol::ResponseID response_id_from_request_id(const protocol::RequestID& id) {
    return std::visit([](const auto& value) -> protocol::ResponseID { return value; }, id);
}

protocol::Value value_from_request_id(const protocol::RequestID& id) {
    return std::visit(
        [](const auto& value) -> protocol::Value {
            using value_t = std::remove_cvref_t<decltype(value)>;
            if constexpr(std::is_same_v<value_t, protocol::integer>) {
                return protocol::Value{static_cast<std::int64_t>(value)};
            } else {
                return protocol::Value{value};
            }
        },
        id);
}

std::optional<protocol::RequestID> request_id_from_response_id(const protocol::ResponseID& id) {
    return std::visit(
        [](const auto& value) -> std::optional<protocol::RequestID> {
            using value_t = std::remove_cvref_t<decltype(value)>;
            if constexpr(std::is_same_v<value_t, protocol::null>) {
                return std::nullopt;
            } else {
                return protocol::RequestID{value};
            }
        },
        id);
}

Result<protocol::ResponseID> parse_response_id(simdjson::ondemand::value& value) {
    simdjson::ondemand::json_type type{};
    auto type_error = value.type().get(type);
    if(type_error != simdjson::SUCCESS) {
        return std::unexpected(RPCError(protocol::ErrorCode::InvalidRequest,
                                        std::string(simdjson::error_message(type_error))));
    }

    if(type == simdjson::ondemand::json_type::number ||
       type == simdjson::ondemand::json_type::string) {
        auto request_id = parse_request_id(value);
        if(!request_id) {
            return std::unexpected(request_id.error());
        }
        return response_id_from_request_id(*request_id);
    }

    if(type == simdjson::ondemand::json_type::null) {
        return protocol::ResponseID{nullptr};
    }

    return std::unexpected(RPCError(protocol::ErrorCode::InvalidRequest,
                                    "request id must be integer, string, or null"));
}

Result<protocol::RequestID> parse_cancel_request_id(std::string_view params_json) {
    auto parsed_params =
        parse_json_value<protocol::Object>(params_json, protocol::ErrorCode::InvalidParams);
    if(!parsed_params) {
        return std::unexpected(parsed_params.error());
    }

    auto id_it = parsed_params->find("id");
    if(id_it == parsed_params->end()) {
        return std::unexpected(
            RPCError(protocol::ErrorCode::InvalidParams, "cancel request params must contain id"));
    }

    const auto& id_variant = static_cast<const protocol::Variant&>(id_it->second);
    if(const auto* integer_id = std::get_if<std::int64_t>(&id_variant)) {
        if(*integer_id < (std::numeric_limits<protocol::integer>::min)() ||
           *integer_id > (std::numeric_limits<protocol::integer>::max)()) {
            return std::unexpected(RPCError(protocol::ErrorCode::InvalidParams,
                                            "cancel request id integer is out of range"));
        }
        return protocol::RequestID{static_cast<protocol::integer>(*integer_id)};
    }

    if(const auto* unsigned_id = std::get_if<std::uint32_t>(&id_variant)) {
        if(*unsigned_id >
           static_cast<std::uint32_t>((std::numeric_limits<protocol::integer>::max)())) {
            return std::unexpected(RPCError(protocol::ErrorCode::InvalidParams,
                                            "cancel request id integer is out of range"));
        }
        return protocol::RequestID{static_cast<protocol::integer>(*unsigned_id)};
    }

    if(const auto* string_id = std::get_if<std::string>(&id_variant)) {
        return protocol::RequestID{*string_id};
    }

    return std::unexpected(RPCError(protocol::ErrorCode::InvalidParams,
                                    "cancel request id must be integer or string"));
}

task<> cancel_after_timeout(std::chrono::milliseconds timeout,
                            std::shared_ptr<cancellation_source> timeout_source,
                            event_loop& loop) {
    co_await sleep(timeout, loop);
    timeout_source->cancel();
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
    protocol::Value params;
};

struct outgoing_notification_message {
    std::string jsonrpc = "2.0";
    std::string method;
    protocol::Value params;
};

struct outgoing_success_response_message {
    std::string jsonrpc = "2.0";
    protocol::RequestID id;
    protocol::Value result;
};

struct outgoing_error_response_message {
    std::string jsonrpc = "2.0";
    protocol::ResponseID id;
    protocol::ResponseError error;
};

}  // namespace

struct Peer::Self {
    struct PendingRequest {
        event ready;
        std::optional<Result<std::string>> response;
    };

    event_loop* loop = nullptr;
    std::unique_ptr<Transport> transport;
    std::unordered_map<std::string, RequestCallback> request_callbacks;
    std::unordered_map<std::string, NotificationCallback> notification_callbacks;
    std::unordered_map<protocol::RequestID, std::shared_ptr<PendingRequest>, request_id_hash>
        pending_requests;
    std::unordered_map<protocol::RequestID, std::shared_ptr<cancellation_source>, request_id_hash>
        incoming_requests;
    protocol::integer next_request_id = 1;
    std::deque<std::string> outgoing_queue;

    bool running = false;
    bool writer_running = false;

    explicit Self(event_loop& external_loop) : loop(&external_loop) {}

    void enqueue_outgoing(std::string payload) {
        outgoing_queue.push_back(std::move(payload));
        if(!writer_running) {
            writer_running = true;
            loop->schedule(write_loop());
        }
    }

    Result<std::string> build_success_response(const protocol::RequestID& id,
                                               std::string_view result_json) {
        auto result =
            parse_json_value<protocol::Value>(result_json, protocol::ErrorCode::InternalError);
        if(!result) {
            return std::unexpected(result.error());
        }

        return detail::serialize_json(outgoing_success_response_message{
            .id = id,
            .result = std::move(*result),
        });
    }

    Result<std::string> build_error_response(const protocol::ResponseID& id,
                                             const RPCError& error) {
        return detail::serialize_json(outgoing_error_response_message{
            .id = id,
            .error =
                protocol::ResponseError{
                                        .code = error.code,
                                        .message = error.message,
                                        .data = error.data,
                                        },
        });
    }

    void send_error(const protocol::ResponseID& id, const RPCError& error) {
        auto response = build_error_response(id, error);
        if(response) {
            enqueue_outgoing(std::move(*response));
        }
    }

    void send_error(const protocol::RequestID& id, const RPCError& error) {
        send_error(response_id_from_request_id(id), error);
    }

    void send_error(const protocol::RequestID& id, protocol::ErrorCode code, std::string message) {
        send_error(id, RPCError(code, std::move(message)));
    }

    void send_error(const protocol::ResponseID& id, protocol::ErrorCode code, std::string message) {
        send_error(id, RPCError(code, std::move(message)));
    }

    void complete_pending_request(const protocol::RequestID& id, Result<std::string> response) {
        if(auto it = pending_requests.find(id); it == pending_requests.end()) {
            return;
        } else {
            auto pending = std::move(it->second);
            pending_requests.erase(it);
            pending->response = std::move(response);
            pending->ready.set();
        }
    }

    void fail_pending_requests(const std::string& message) {
        if(pending_requests.empty()) {
            return;
        }

        std::vector<std::shared_ptr<PendingRequest>> pending;
        pending.reserve(pending_requests.size());
        for(auto& pending_entry: pending_requests) {
            pending.push_back(pending_entry.second);
        }
        pending_requests.clear();

        for(auto& state: pending) {
            state->response = std::unexpected(RPCError(message));
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
        if(method == "$/cancelRequest") {
            auto cancel_id = parse_cancel_request_id(params_json);
            if(!cancel_id) {
                return;
            }

            if(auto it = incoming_requests.find(*cancel_id); it == incoming_requests.end()) {
                return;
            } else if(it->second) {
                it->second->cancel();
            }
            return;
        }

        if(auto it = notification_callbacks.find(method); it == notification_callbacks.end()) {
            return;
        } else {
            it->second(params_json);
        }
    }

    task<> run_request(protocol::RequestID id,
                       RequestCallback callback,
                       std::string params_json,
                       cancellation_token token) {
        auto guarded_result = co_await with_token(token, callback(id, params_json, token));
        incoming_requests.erase(id);

        if(!guarded_result.has_value()) {
            send_error(id, protocol::ErrorCode::RequestCancelled, "request cancelled");
            co_return;
        }

        auto result_json = std::move(*guarded_result);
        if(!result_json) {
            send_error(id, result_json.error());
            co_return;
        }

        auto response = build_success_response(id, *result_json);
        if(!response) {
            send_error(id, RPCError(protocol::ErrorCode::InternalError, response.error().message));
            co_return;
        }

        enqueue_outgoing(std::move(*response));
    }

    void dispatch_request(const std::string& method,
                          const protocol::RequestID& id,
                          std::string_view params_json) {
        if(incoming_requests.contains(id)) {
            send_error(id, protocol::ErrorCode::InvalidRequest, "duplicate request id");
            return;
        }

        if(auto it = request_callbacks.find(method); it == request_callbacks.end()) {
            send_error(id, protocol::ErrorCode::MethodNotFound, "method not found: " + method);
            return;
        } else {
            auto callback = it->second;
            auto cancel_source = std::make_shared<cancellation_source>();
            incoming_requests.insert_or_assign(id, cancel_source);
            auto task = run_request(id,
                                    std::move(callback),
                                    std::string(params_json),
                                    cancel_source->token());
            loop->schedule(std::move(task));
        }
    }

    void dispatch_response(const protocol::RequestID& id,
                           const std::optional<std::string_view>& result_json,
                           const std::optional<std::string_view>& error_json) {
        if(error_json.has_value()) {
            auto parsed_error =
                parse_json_value<protocol::ResponseError>(*error_json,
                                                          protocol::ErrorCode::InvalidRequest);
            if(!parsed_error) {
                complete_pending_request(id, std::unexpected(parsed_error.error()));
                return;
            }

            auto response_error = std::move(*parsed_error);
            complete_pending_request(id,
                                     std::unexpected(RPCError(response_error.code,
                                                              std::move(response_error.message),
                                                              std::move(response_error.data))));
            return;
        }

        if(!result_json.has_value()) {
            complete_pending_request(id,
                                     std::unexpected(RPCError(protocol::ErrorCode::InvalidRequest,
                                                              "response is missing result")));
            return;
        }

        complete_pending_request(id, std::string(*result_json));
    }

    void dispatch_incoming_message(std::string_view payload) {
        auto send_protocol_error = [this](std::optional<protocol::ResponseID> id,
                                          protocol::ErrorCode code,
                                          std::string message) {
            auto response_id = id.value_or(protocol::ResponseID{nullptr});
            send_error(response_id, code, std::move(message));
        };

        simdjson::ondemand::parser parser;
        simdjson::padded_string json(payload);

        simdjson::ondemand::document document{};
        auto document_result = parser.iterate(json);
        auto document_error = std::move(document_result).get(document);
        if(document_error != simdjson::SUCCESS) {
            send_protocol_error(std::nullopt,
                                protocol::ErrorCode::ParseError,
                                std::string(simdjson::error_message(document_error)));
            return;
        }

        simdjson::ondemand::object object{};
        auto object_result = document.get_object();
        auto object_error = std::move(object_result).get(object);
        if(object_error != simdjson::SUCCESS) {
            auto code = object_error == simdjson::INCORRECT_TYPE
                            ? protocol::ErrorCode::InvalidRequest
                            : protocol::ErrorCode::ParseError;
            send_protocol_error(std::nullopt,
                                code,
                                std::string(simdjson::error_message(object_error)));
            return;
        }

        std::optional<std::string> method;
        std::optional<protocol::ResponseID> id;
        std::optional<std::string_view> params_json;
        std::optional<std::string_view> result_json;
        std::optional<std::string_view> error_json;
        auto fail_message = [this, &method, &send_protocol_error](
                                std::optional<protocol::ResponseID> id,
                                protocol::ErrorCode code,
                                std::string message) {
            if(!method.has_value() && id.has_value()) {
                if(auto request_id = request_id_from_response_id(*id); request_id.has_value()) {
                    complete_pending_request(*request_id,
                                             std::unexpected(RPCError(code, std::move(message))));
                }
                return;
            }

            send_protocol_error(id, code, std::move(message));
        };

        for(auto field_result: object) {
            simdjson::ondemand::field field{};
            auto field_error = std::move(field_result).get(field);
            if(field_error != simdjson::SUCCESS) {
                fail_message(id,
                             protocol::ErrorCode::InvalidRequest,
                             std::string(simdjson::error_message(field_error)));
                return;
            }

            std::string_view key;
            auto key_error = field.unescaped_key().get(key);
            if(key_error != simdjson::SUCCESS) {
                fail_message(id,
                             protocol::ErrorCode::InvalidRequest,
                             std::string(simdjson::error_message(key_error)));
                return;
            }

            auto value = field.value();
            if(key == "method") {
                std::string_view method_text;
                auto method_error = value.get_string().get(method_text);
                if(method_error != simdjson::SUCCESS) {
                    send_protocol_error(id,
                                        protocol::ErrorCode::InvalidRequest,
                                        std::string(simdjson::error_message(method_error)));
                    return;
                }
                method = std::string(method_text);
                continue;
            }

            if(key == "id") {
                auto parsed_id = parse_response_id(value);
                if(!parsed_id) {
                    send_protocol_error(std::nullopt,
                                        protocol::ErrorCode::InvalidRequest,
                                        parsed_id.error().message);
                    return;
                }
                id = std::move(*parsed_id);
                continue;
            }

            std::string_view raw_json;
            auto raw_error = value.raw_json().get(raw_json);
            if(raw_error != simdjson::SUCCESS) {
                fail_message(id,
                             protocol::ErrorCode::InvalidRequest,
                             std::string(simdjson::error_message(raw_error)));
                return;
            }

            if(key == "params") {
                params_json = raw_json;
                continue;
            }

            if(key == "result") {
                result_json = raw_json;
                continue;
            }

            if(key == "error") {
                error_json = raw_json;
                continue;
            }
        }

        if(!document.at_end()) {
            fail_message(id,
                         protocol::ErrorCode::InvalidRequest,
                         "trailing content after JSON-RPC message");
            return;
        }

        if(method.has_value()) {
            auto params = params_json.value_or(std::string_view{});
            if(id.has_value()) {
                auto request_id = request_id_from_response_id(*id);
                if(!request_id) {
                    send_protocol_error(std::nullopt,
                                        protocol::ErrorCode::InvalidRequest,
                                        "request id must be integer or string");
                    return;
                }
                dispatch_request(*method, *request_id, params);
            } else {
                dispatch_notification(*method, params);
            }
            return;
        }

        if(id.has_value()) {
            auto request_id = request_id_from_response_id(*id);
            if(!request_id) {
                return;
            }

            if(result_json.has_value() == error_json.has_value()) {
                complete_pending_request(
                    *request_id,
                    std::unexpected(
                        RPCError(protocol::ErrorCode::InvalidRequest,
                                 "response must contain exactly one of result or error")));
                return;
            }

            dispatch_response(*request_id, result_json, error_json);
            return;
        }

        send_protocol_error(std::nullopt,
                            protocol::ErrorCode::InvalidRequest,
                            "message must contain method or id");
        return;
    }
};

Peer::Peer(event_loop& loop, std::unique_ptr<Transport> transport) :
    self(std::make_unique<Self>(loop)) {
    self->transport = std::move(transport);
}

Peer::~Peer() = default;

task<> Peer::run() {
    if(!self || !self->transport || self->running) {
        co_return;
    }

    self->running = true;

    while(self->transport) {
        auto payload = co_await self->transport->read_message();
        if(!payload.has_value()) {
            self->fail_pending_requests("transport closed");
            break;
        }

        self->dispatch_incoming_message(*payload);
    }

    self->running = false;
}

Result<void> Peer::close_output() {
    if(!self || !self->transport) {
        return std::unexpected("transport is null");
    }

    return self->transport->close_output();
}

void Peer::register_request_callback(std::string_view method, RequestCallback callback) {
    self->request_callbacks.insert_or_assign(std::string(method), std::move(callback));
}

void Peer::register_notification_callback(std::string_view method, NotificationCallback callback) {
    self->notification_callbacks.insert_or_assign(std::string(method), std::move(callback));
}

task<Result<std::string>> Peer::send_request_json(std::string_view method,
                                                  std::string params_json) {
    co_return co_await send_request_json(method, std::move(params_json), cancellation_token{});
}

task<Result<std::string>> Peer::send_request_json(std::string_view method,
                                                  std::string params_json,
                                                  cancellation_token token) {
    if(!self || !self->transport) {
        co_return std::unexpected("transport is null");
    }

    if(token.cancelled()) {
        co_return std::unexpected(
            RPCError(protocol::ErrorCode::RequestCancelled, "request cancelled"));
    }

    auto params =
        parse_json_value<protocol::Value>(params_json, protocol::ErrorCode::InternalError);
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

    auto wait_pending = [](const std::shared_ptr<Self::PendingRequest>& state) -> task<> {
        co_await state->ready.wait();
    };
    auto wait_result = co_await with_token(token, wait_pending(pending));
    if(!wait_result.has_value()) {
        if(auto it = self->pending_requests.find(request_id); it != self->pending_requests.end()) {
            self->pending_requests.erase(it);
            protocol::Object cancel_params;
            cancel_params.insert_or_assign("id", value_from_request_id(request_id));
            auto cancel_request = detail::serialize_json(outgoing_notification_message{
                .method = "$/cancelRequest",
                .params = protocol::Value(std::move(cancel_params)),
            });
            if(cancel_request) {
                self->enqueue_outgoing(std::move(*cancel_request));
            }
        }
        co_return std::unexpected(
            RPCError(protocol::ErrorCode::RequestCancelled, "request cancelled"));
    }

    if(!pending->response.has_value()) {
        co_return std::unexpected("request was not completed");
    }

    co_return std::move(*pending->response);
}

task<Result<std::string>> Peer::send_request_json(std::string_view method,
                                                  std::string params_json,
                                                  std::chrono::milliseconds timeout) {
    if(timeout <= std::chrono::milliseconds::zero()) {
        co_return std::unexpected(
            RPCError(protocol::ErrorCode::RequestCancelled, "request timed out"));
    }

    auto timeout_source = std::make_shared<cancellation_source>();
    if(auto* loop_ptr = self ? self->loop : nullptr; loop_ptr != nullptr) {
        loop_ptr->schedule(cancel_after_timeout(timeout, timeout_source, *loop_ptr));
    }

    auto result =
        co_await send_request_json(method, std::move(params_json), timeout_source->token());
    if(!result &&
       result.error().code ==
           static_cast<protocol::integer>(protocol::ErrorCode::RequestCancelled) &&
       timeout_source->cancelled()) {
        co_return std::unexpected(
            RPCError(protocol::ErrorCode::RequestCancelled, "request timed out"));
    }

    co_return result;
}

Result<void> Peer::send_notification_json(std::string_view method, std::string params_json) {
    if(!self || !self->transport) {
        return std::unexpected("transport is null");
    }

    auto params =
        parse_json_value<protocol::Value>(params_json, protocol::ErrorCode::InternalError);
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

}  // namespace eventide::jsonrpc
