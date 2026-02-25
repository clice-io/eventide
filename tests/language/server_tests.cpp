#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/common/compiler.h"
#include "eventide/async/sync.h"
#include "eventide/serde/simdjson/deserializer.h"
#include "eventide/language/server.h"

namespace eventide::language {

struct AddParams {
    std::int64_t a = 0;
    std::int64_t b = 0;
};

struct AddResult {
    std::int64_t sum = 0;
};

struct NoteParams {
    std::string text;
};

struct CustomAddParams {
    std::int64_t a = 0;
    std::int64_t b = 0;
};

struct CustomNoteParams {
    std::string text;
};

struct RpcResponse {
    std::string jsonrpc;
    protocol::variant<protocol::integer, protocol::string> id;
    protocol::optional<AddResult> result = {};
};

struct RpcRequest {
    std::string jsonrpc;
    protocol::variant<protocol::integer, protocol::string> id;
    std::string method;
    AddParams params;
};

struct RpcNotification {
    std::string jsonrpc;
    std::string method;
    NoteParams params;
};

class FakeTransport final : public Transport {
public:
    explicit FakeTransport(std::vector<std::string> incoming) :
        incoming_messages(std::move(incoming)) {}

    task<std::optional<std::string>> read_message() override {
        if(read_index >= incoming_messages.size()) {
            co_return std::nullopt;
        }
        co_return incoming_messages[read_index++];
    }

    task<bool> write_message(std::string_view payload) override {
        outgoing_messages.emplace_back(payload);
        co_return true;
    }

    const std::vector<std::string>& outgoing() const {
        return outgoing_messages;
    }

private:
    std::vector<std::string> incoming_messages;
    std::vector<std::string> outgoing_messages;
    std::size_t read_index = 0;
};

class ScriptedTransport final : public Transport {
public:
    using WriteHook = std::function<void(std::string_view, ScriptedTransport&)>;

    ScriptedTransport(std::vector<std::string> incoming, WriteHook hook) :
        incoming_messages(std::move(incoming)), write_hook(std::move(hook)) {
        if(!incoming_messages.empty()) {
            readable.set();
        }
    }

    task<std::optional<std::string>> read_message() override {
        while(read_index >= incoming_messages.size()) {
            if(closed) {
                co_return std::nullopt;
            }

            co_await readable.wait();
            readable.reset();
        }

        co_return incoming_messages[read_index++];
    }

    task<bool> write_message(std::string_view payload) override {
        outgoing_messages.emplace_back(payload);
        if(write_hook) {
            write_hook(payload, *this);
        }
        co_return true;
    }

    void push_incoming(std::string payload) {
        incoming_messages.push_back(std::move(payload));
        readable.set();
    }

    void close() {
        closed = true;
        readable.set();
    }

    const std::vector<std::string>& outgoing() const {
        return outgoing_messages;
    }

private:
    std::vector<std::string> incoming_messages;
    std::vector<std::string> outgoing_messages;
    std::size_t read_index = 0;
    WriteHook write_hook;
    event readable;
    bool closed = false;
};

}  // namespace eventide::language

namespace eventide::language::protocol {

template <>
struct RequestTraits<AddParams> {
    using Result = AddResult;
    constexpr inline static std::string_view method = "test/add";
};

template <>
struct NotificationTraits<NoteParams> {
    constexpr inline static std::string_view method = "test/note";
};

}  // namespace eventide::language::protocol

namespace eventide::language {

TEST_SUITE(language_server) {

TEST_CASE(traits_registration_and_dispatch_order) {
// Visual Studio issue:
// https://developercommunity.visualstudio.com/t/Unable-to-destroy-C20-coroutine-in-fin/10657377
#if EVENTIDE_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
    skip();
    return;
#endif
    auto transport = std::make_unique<FakeTransport>(std::vector<std::string>{
        R"({"jsonrpc":"2.0","id":1,"method":"test/add","params":{"a":2,"b":3}})",
        R"({"jsonrpc":"2.0","method":"test/note","params":{"text":"first"}})",
        R"({"jsonrpc":"2.0","method":"test/note","params":{"text":"second"}})",
    });
    auto* transport_ptr = transport.get();

    LanguageServer server(std::move(transport));
    std::vector<std::string> order;
    bool second_saw_first = false;
    bool first_seen = false;

    server.on_request([&](RequestContext&, const AddParams& params) -> RequestResult<AddParams> {
        order.emplace_back("request");
        co_return AddResult{.sum = params.a + params.b};
    });

    server.on_notification([&](const NoteParams& params) {
        if(params.text == "first") {
            first_seen = true;
            order.emplace_back("note:first");
            return;
        }
        if(params.text == "second") {
            second_saw_first = first_seen;
            order.emplace_back("note:second");
        }
    });

    EXPECT_EQ(server.start(), 0);

    ASSERT_EQ(order.size(), 3U);
    EXPECT_EQ(order[0], "note:first");
    EXPECT_EQ(order[1], "note:second");
    EXPECT_EQ(order[2], "request");
    EXPECT_TRUE(second_saw_first);

    ASSERT_EQ(transport_ptr->outgoing().size(), 1U);
    auto response = serde::json::simd::from_json<RpcResponse>(transport_ptr->outgoing().front());
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->jsonrpc, "2.0");
    EXPECT_EQ(std::get<protocol::integer>(response->id), 1);
    ASSERT_TRUE(response->result.has_value());
    EXPECT_EQ(response->result->sum, 5);
}

TEST_CASE(explicit_method_registration) {
#if EVENTIDE_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
    skip();
    return;
#endif
    auto transport = std::make_unique<FakeTransport>(std::vector<std::string>{
        R"({"jsonrpc":"2.0","id":2,"method":"custom/add","params":{"a":7,"b":8}})",
        R"({"jsonrpc":"2.0","method":"custom/note","params":{"text":"hello"}})",
    });
    auto* transport_ptr = transport.get();

    LanguageServer server(std::move(transport));
    std::string request_method;
    std::vector<std::string> notifications;

    server.on_request(
        "custom/add",
        [&](RequestContext& context, const AddParams& params) -> RequestResult<AddParams> {
            request_method = std::string(context.method);
            co_return AddResult{.sum = params.a + params.b};
        });

    server.on_notification("custom/note",
                           [&](const NoteParams& params) { notifications.push_back(params.text); });

    EXPECT_EQ(server.start(), 0);

    EXPECT_EQ(request_method, "custom/add");
    ASSERT_EQ(notifications.size(), 1U);
    EXPECT_EQ(notifications.front(), "hello");

    ASSERT_EQ(transport_ptr->outgoing().size(), 1U);
    auto response = serde::json::simd::from_json<RpcResponse>(transport_ptr->outgoing().front());
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(std::get<protocol::integer>(response->id), 2);
    ASSERT_TRUE(response->result.has_value());
    EXPECT_EQ(response->result->sum, 15);
}

TEST_CASE(send_request_and_notification_apis) {
#if EVENTIDE_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
    skip();
    return;
#endif
    auto transport = std::make_unique<ScriptedTransport>(
        std::vector<std::string>{
            R"({"jsonrpc":"2.0","id":7,"method":"test/add","params":{"a":2,"b":3}})",
        },
        [](std::string_view payload, ScriptedTransport& channel) {
            if(payload.find("\"method\":\"client/add/context\"") != std::string_view::npos) {
                channel.push_incoming(R"({"jsonrpc":"2.0","id":1,"result":{"sum":9}})");
                return;
            }

            if(payload.find("\"method\":\"client/add/server\"") != std::string_view::npos) {
                channel.push_incoming(R"({"jsonrpc":"2.0","id":2,"result":{"sum":4}})");
                return;
            }

            if(payload.find("\"id\":7") != std::string_view::npos &&
               payload.find("\"result\"") != std::string_view::npos) {
                channel.close();
            }
        });
    auto* transport_ptr = transport.get();

    LanguageServer server(std::move(transport));
    std::string request_method;
    protocol::integer request_id = 0;

    server.on_request([&](RequestContext& context,
                          const AddParams& params) -> RequestResult<AddParams> {
        request_method = std::string(context.method);
        request_id = std::get<protocol::integer>(context.id);

        auto notify_from_context =
            context->send_notification("client/note/context", CustomNoteParams{.text = "context"});
        if(!notify_from_context) {
            co_return std::unexpected(notify_from_context.error());
        }

        auto notify_from_server =
            server.send_notification("client/note/server", CustomNoteParams{.text = "server"});
        if(!notify_from_server) {
            co_return std::unexpected(notify_from_server.error());
        }

        auto context_result = co_await context->send_request<AddResult>(
            "client/add/context",
            CustomAddParams{.a = params.a, .b = params.b});
        if(!context_result) {
            co_return std::unexpected(context_result.error());
        }

        auto server_result =
            co_await server.send_request<AddResult>("client/add/server",
                                                    CustomAddParams{.a = params.b, .b = 1});
        if(!server_result) {
            co_return std::unexpected(server_result.error());
        }

        co_return AddResult{.sum = context_result->sum + server_result->sum};
    });

    EXPECT_EQ(server.start(), 0);

    EXPECT_EQ(request_method, "test/add");
    EXPECT_EQ(request_id, 7);

    const auto& outgoing = transport_ptr->outgoing();
    ASSERT_EQ(outgoing.size(), 5U);

    auto note_from_context = serde::json::simd::from_json<RpcNotification>(outgoing[0]);
    ASSERT_TRUE(note_from_context.has_value());
    EXPECT_EQ(note_from_context->jsonrpc, "2.0");
    EXPECT_EQ(note_from_context->method, "client/note/context");
    EXPECT_EQ(note_from_context->params.text, "context");

    auto note_from_server = serde::json::simd::from_json<RpcNotification>(outgoing[1]);
    ASSERT_TRUE(note_from_server.has_value());
    EXPECT_EQ(note_from_server->jsonrpc, "2.0");
    EXPECT_EQ(note_from_server->method, "client/note/server");
    EXPECT_EQ(note_from_server->params.text, "server");

    auto request_from_context = serde::json::simd::from_json<RpcRequest>(outgoing[2]);
    ASSERT_TRUE(request_from_context.has_value());
    EXPECT_EQ(request_from_context->jsonrpc, "2.0");
    EXPECT_EQ(std::get<protocol::integer>(request_from_context->id), 1);
    EXPECT_EQ(request_from_context->method, "client/add/context");
    EXPECT_EQ(request_from_context->params.a, 2);
    EXPECT_EQ(request_from_context->params.b, 3);

    auto request_from_server = serde::json::simd::from_json<RpcRequest>(outgoing[3]);
    ASSERT_TRUE(request_from_server.has_value());
    EXPECT_EQ(request_from_server->jsonrpc, "2.0");
    EXPECT_EQ(std::get<protocol::integer>(request_from_server->id), 2);
    EXPECT_EQ(request_from_server->method, "client/add/server");
    EXPECT_EQ(request_from_server->params.a, 3);
    EXPECT_EQ(request_from_server->params.b, 1);

    auto final_response = serde::json::simd::from_json<RpcResponse>(outgoing[4]);
    ASSERT_TRUE(final_response.has_value());
    EXPECT_EQ(final_response->jsonrpc, "2.0");
    EXPECT_EQ(std::get<protocol::integer>(final_response->id), 7);
    ASSERT_TRUE(final_response->result.has_value());
    EXPECT_EQ(final_response->result->sum, 13);
}

};  // TEST_SUITE(language_server)

}  // namespace eventide::language
