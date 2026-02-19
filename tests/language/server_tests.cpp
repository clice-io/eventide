#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "zest/zest.h"
#include "eventide/compiler.h"
#include "language/server.h"
#include "serde/simdjson/deserializer.h"

namespace language::testing {

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

struct RpcResponse {
    std::string jsonrpc;
    protocol::variant<protocol::integer, protocol::string> id;
    protocol::optional<AddResult> result = {};
};

class FakeTransport final : public Transport {
public:
    explicit FakeTransport(std::vector<std::string> incoming) :
        incoming_messages(std::move(incoming)) {}

    et::task<std::optional<std::string>> read_message() override {
        if(read_index >= incoming_messages.size()) {
            co_return std::nullopt;
        }
        co_return incoming_messages[read_index++];
    }

    et::task<bool> write_message(std::string_view payload) override {
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

}  // namespace language::testing

namespace language::protocol {

template <>
struct RequestTraits<language::testing::AddParams> {
    using Result = language::testing::AddResult;
    constexpr inline static std::string_view method = "test/add";
};

template <>
struct NotificationTraits<language::testing::NoteParams> {
    constexpr inline static std::string_view method = "test/note";
};

}  // namespace language::protocol

namespace language::testing {

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

};  // TEST_SUITE(language_server)

}  // namespace language::testing
