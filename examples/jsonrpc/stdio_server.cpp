#include <cstdint>
#include <print>
#include <string>
#include <string_view>

#include "eventide/jsonrpc/peer.h"

namespace et = eventide;
namespace jsonrpc = et::jsonrpc;

namespace example {

constexpr std::string_view add_method = "example/add";
constexpr std::string_view log_method = "example/log";

struct AddParams {
    std::int64_t a = 0;
    std::int64_t b = 0;
};

struct AddResult {
    std::int64_t sum = 0;
};

struct LogParams {
    std::string text;
};

}  // namespace example

int main() {
    jsonrpc::Peer peer;

    peer.on_request(example::add_method,
                    [](jsonrpc::RequestContext&, const example::AddParams& params)
                        -> jsonrpc::RequestResult<example::AddParams, example::AddResult> {
                        co_return example::AddResult{.sum = params.a + params.b};
                    });

    peer.on_notification(example::log_method, [](const example::LogParams& params) {
        std::println(stderr, "[example/log] {}", params.text);
    });

    std::println(stderr, "JSON-RPC stdio example is ready.");
    std::println(stderr, "Request method: {}", example::add_method);
    std::println(stderr, "Notification method: {}", example::log_method);

    return peer.start();
}
