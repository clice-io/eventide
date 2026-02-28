#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "eventide/jsonrpc/peer.h"
#include "eventide/async/sync.h"

namespace et = eventide;
namespace jsonrpc = et::jsonrpc;

namespace example {

constexpr std::string_view add_method = "example/add";
constexpr std::string_view note_method = "example/note";
constexpr std::string_view client_add_method = "client/add";

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

struct ClientAddParams {
    std::int64_t a = 0;
    std::int64_t b = 0;
};

class ScriptedTransport final : public jsonrpc::Transport {
public:
    using WriteHook = std::function<void(std::string_view, ScriptedTransport&)>;

    ScriptedTransport(std::vector<std::string> incoming, WriteHook hook) :
        incoming_messages(std::move(incoming)), write_hook(std::move(hook)) {
        if(!incoming_messages.empty()) {
            readable.set();
        }
    }

    et::task<std::optional<std::string>> read_message() override {
        while(read_index >= incoming_messages.size()) {
            if(closed) {
                co_return std::nullopt;
            }

            co_await readable.wait();
            readable.reset();
        }

        co_return incoming_messages[read_index++];
    }

    et::task<bool> write_message(std::string_view payload) override {
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
    et::event readable;
    bool closed = false;
};

}  // namespace example

int main() {
    auto transport = std::make_unique<example::ScriptedTransport>(
        std::vector<std::string>{
            R"({"jsonrpc":"2.0","id":7,"method":"example/add","params":{"a":2,"b":3}})",
        },
        [](std::string_view payload, example::ScriptedTransport& channel) {
            if(payload.find("\"method\":\"client/add\"") != std::string_view::npos) {
                channel.push_incoming(R"({"jsonrpc":"2.0","id":1,"result":{"sum":4}})");
                return;
            }

            if(payload.find("\"id\":7") != std::string_view::npos &&
               payload.find("\"result\"") != std::string_view::npos) {
                channel.close();
            }
        });
    auto* transport_ptr = transport.get();

    jsonrpc::Peer peer(std::move(transport));

    peer.on_request(example::add_method,
                    [](jsonrpc::RequestContext& context, const example::AddParams& params)
                        -> jsonrpc::RequestResult<example::AddParams, example::AddResult> {
                        auto notify_status = context->send_notification(
                            example::note_method,
                            example::NoteParams{.text = "handling request"});
                        if(!notify_status) {
                            co_return std::unexpected(notify_status.error());
                        }

                        auto remote_sum = co_await context->send_request<example::AddResult>(
                            example::client_add_method,
                            example::ClientAddParams{.a = params.b, .b = 1});
                        if(!remote_sum) {
                            co_return std::unexpected(remote_sum.error());
                        }

                        co_return example::AddResult{.sum = params.a + params.b + remote_sum->sum};
                    });

    auto status = peer.start();
    if(status != 0) {
        std::println(stderr, "peer exited with status {}", status);
        return status;
    }

    std::println("Outgoing messages:");
    for(const auto& message: transport_ptr->outgoing()) {
        std::println("{}", message);
    }

    return 0;
}
