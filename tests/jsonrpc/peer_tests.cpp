#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "eventide/jsonrpc/peer.h"
#include "eventide/zest/zest.h"
#include "eventide/common/compiler.h"
#include "eventide/async/loop.h"
#include "eventide/async/stream.h"
#include "eventide/async/sync.h"
#include "eventide/async/watcher.h"
#include "eventide/serde/simdjson/deserializer.h"

#ifdef _WIN32
#include <BaseTsd.h>
#include <fcntl.h>
#include <io.h>
using ssize_t = SSIZE_T;
#else
#include <unistd.h>
#endif

namespace eventide::jsonrpc {

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
    protocol::RequestID id;
    std::optional<AddResult> result = {};
};

struct RpcRequest {
    std::string jsonrpc;
    protocol::RequestID id;
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

struct PendingAddResult {
    std::expected<AddResult, std::string> value = std::unexpected("request not completed");
};

namespace {

#ifdef _WIN32
int create_pipe_fds(int fds[2]) {
    return _pipe(fds, 4096, _O_BINARY);
}

int close_fd(int fd) {
    return _close(fd);
}

ssize_t write_fd(int fd, const char* data, size_t len) {
    return _write(fd, data, static_cast<unsigned int>(len));
}
#else
int create_pipe_fds(int fds[2]) {
    return ::pipe(fds);
}

int close_fd(int fd) {
    return ::close(fd);
}

ssize_t write_fd(int fd, const char* data, size_t len) {
    return ::write(fd, data, len);
}
#endif

std::string frame(std::string_view payload) {
    std::string out;
    out.reserve(payload.size() + 32);
    out.append("Content-Length: ");
    out.append(std::to_string(payload.size()));
    out.append("\r\n\r\n");
    out.append(payload);
    return out;
}

task<> complete_request(Peer& peer, PendingAddResult& out) {
    out.value =
        co_await peer.send_request<AddResult>("worker/build", CustomAddParams{.a = 2, .b = 3});
    if(!peer.close_output() && out.value.has_value()) {
        out.value = std::unexpected("failed to close peer output");
    }
    co_return;
}

task<> write_notification_then_response(int fd, event_loop& loop) {
    co_await sleep(std::chrono::milliseconds{1}, loop);

    const auto note = frame(R"({"jsonrpc":"2.0","method":"test/note","params":{"text":"first"}})");
    auto note_written = write_fd(fd, note.data(), note.size());
    if(note_written != static_cast<ssize_t>(note.size())) {
        close_fd(fd);
        co_return;
    }

    co_await sleep(std::chrono::milliseconds{1}, loop);

    const auto response = frame(R"({"jsonrpc":"2.0","id":1,"result":{"sum":9}})");
    auto response_written = write_fd(fd, response.data(), response.size());
    if(response_written != static_cast<ssize_t>(response.size())) {
        close_fd(fd);
        co_return;
    }

    close_fd(fd);
    co_return;
}

}  // namespace

}  // namespace eventide::jsonrpc

namespace eventide::jsonrpc::protocol {

template <>
struct RequestTraits<AddParams> {
    using Result = AddResult;
    constexpr inline static std::string_view method = "test/add";
};

template <>
struct NotificationTraits<NoteParams> {
    constexpr inline static std::string_view method = "test/note";
};

}  // namespace eventide::jsonrpc::protocol

namespace eventide::jsonrpc {

TEST_SUITE(jsonrpc_peer) {

TEST_CASE(traits_registration_and_dispatch_order) {
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

    Peer peer(std::move(transport));
    std::vector<std::string> order;
    bool second_saw_first = false;
    bool first_seen = false;

    peer.on_request([&](RequestContext&, const AddParams& params) -> RequestResult<AddParams> {
        order.emplace_back("request");
        co_return AddResult{.sum = params.a + params.b};
    });

    peer.on_notification([&](const NoteParams& params) {
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

    EXPECT_EQ(peer.start(), 0);

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

TEST_CASE(stream_transport_notification_then_response) {
#if EVENTIDE_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
    skip();
    return;
#endif
    event_loop loop;

    int incoming_fds[2] = {-1, -1};
    int outgoing_fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe_fds(incoming_fds), 0);
    ASSERT_EQ(create_pipe_fds(outgoing_fds), 0);

    auto input = pipe::open(incoming_fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());
    auto output = pipe::open(outgoing_fds[1], pipe::options{}, loop);
    ASSERT_TRUE(output.has_value());

    auto transport =
        std::make_unique<StreamTransport>(stream(std::move(*input)), stream(std::move(*output)));
    Peer peer(loop, std::move(transport));

    std::vector<std::string> seen_notes;
    peer.on_notification("test/note",
                         [&](const NoteParams& params) { seen_notes.push_back(params.text); });

    PendingAddResult request_result;
    auto request = complete_request(peer, request_result);
    auto remote = write_notification_then_response(incoming_fds[1], loop);

    loop.schedule(request);
    loop.schedule(remote);

    EXPECT_EQ(peer.start(), 0);

    ASSERT_TRUE(request_result.value.has_value());
    EXPECT_EQ(request_result.value->sum, 9);
    ASSERT_EQ(seen_notes.size(), 1U);
    EXPECT_EQ(seen_notes.front(), "first");

    ASSERT_EQ(close_fd(outgoing_fds[0]), 0);
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

    Peer peer(std::move(transport));
    std::string request_method;
    std::vector<std::string> notifications;

    peer.on_request(
        "custom/add",
        [&](RequestContext& context, const AddParams& params) -> RequestResult<AddParams> {
            request_method = std::string(context.method);
            co_return AddResult{.sum = params.a + params.b};
        });

    peer.on_notification("custom/note",
                         [&](const NoteParams& params) { notifications.push_back(params.text); });

    EXPECT_EQ(peer.start(), 0);

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

            if(payload.find("\"method\":\"client/add/peer\"") != std::string_view::npos) {
                channel.push_incoming(R"({"jsonrpc":"2.0","id":2,"result":{"sum":4}})");
                return;
            }

            if(payload.find("\"id\":7") != std::string_view::npos &&
               payload.find("\"result\"") != std::string_view::npos) {
                channel.close();
            }
        });
    auto* transport_ptr = transport.get();

    Peer peer(std::move(transport));
    std::string request_method;
    protocol::integer request_id = 0;

    peer.on_request([&](RequestContext& context,
                        const AddParams& params) -> RequestResult<AddParams> {
        request_method = std::string(context.method);
        request_id = std::get<protocol::integer>(context.id);

        auto notify_from_context =
            context->send_notification("client/note/context", CustomNoteParams{.text = "context"});
        if(!notify_from_context) {
            co_return std::unexpected(notify_from_context.error());
        }

        auto notify_from_peer =
            peer.send_notification("client/note/peer", CustomNoteParams{.text = "peer"});
        if(!notify_from_peer) {
            co_return std::unexpected(notify_from_peer.error());
        }

        auto context_result = co_await context->send_request<AddResult>(
            "client/add/context",
            CustomAddParams{.a = params.a, .b = params.b});
        if(!context_result) {
            co_return std::unexpected(context_result.error());
        }

        auto peer_result =
            co_await peer.send_request<AddResult>("client/add/peer",
                                                  CustomAddParams{.a = params.b, .b = 1});
        if(!peer_result) {
            co_return std::unexpected(peer_result.error());
        }

        co_return AddResult{.sum = context_result->sum + peer_result->sum};
    });

    EXPECT_EQ(peer.start(), 0);

    EXPECT_EQ(request_method, "test/add");
    EXPECT_EQ(request_id, 7);

    const auto& outgoing = transport_ptr->outgoing();
    ASSERT_EQ(outgoing.size(), 5U);

    auto note_from_context = serde::json::simd::from_json<RpcNotification>(outgoing[0]);
    ASSERT_TRUE(note_from_context.has_value());
    EXPECT_EQ(note_from_context->jsonrpc, "2.0");
    EXPECT_EQ(note_from_context->method, "client/note/context");
    EXPECT_EQ(note_from_context->params.text, "context");

    auto note_from_peer = serde::json::simd::from_json<RpcNotification>(outgoing[1]);
    ASSERT_TRUE(note_from_peer.has_value());
    EXPECT_EQ(note_from_peer->jsonrpc, "2.0");
    EXPECT_EQ(note_from_peer->method, "client/note/peer");
    EXPECT_EQ(note_from_peer->params.text, "peer");

    auto request_from_context = serde::json::simd::from_json<RpcRequest>(outgoing[2]);
    ASSERT_TRUE(request_from_context.has_value());
    EXPECT_EQ(request_from_context->jsonrpc, "2.0");
    EXPECT_EQ(std::get<protocol::integer>(request_from_context->id), 1);
    EXPECT_EQ(request_from_context->method, "client/add/context");
    EXPECT_EQ(request_from_context->params.a, 2);
    EXPECT_EQ(request_from_context->params.b, 3);

    auto request_from_peer = serde::json::simd::from_json<RpcRequest>(outgoing[3]);
    ASSERT_TRUE(request_from_peer.has_value());
    EXPECT_EQ(request_from_peer->jsonrpc, "2.0");
    EXPECT_EQ(std::get<protocol::integer>(request_from_peer->id), 2);
    EXPECT_EQ(request_from_peer->method, "client/add/peer");
    EXPECT_EQ(request_from_peer->params.a, 3);
    EXPECT_EQ(request_from_peer->params.b, 1);

    auto final_response = serde::json::simd::from_json<RpcResponse>(outgoing[4]);
    ASSERT_TRUE(final_response.has_value());
    EXPECT_EQ(final_response->jsonrpc, "2.0");
    EXPECT_EQ(std::get<protocol::integer>(final_response->id), 7);
    ASSERT_TRUE(final_response->result.has_value());
    EXPECT_EQ(final_response->result->sum, 13);
}

};  // TEST_SUITE(jsonrpc_peer)

}  // namespace eventide::jsonrpc
