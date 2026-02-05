#pragma once

#include <string>
#include <string_view>

#include "eventide/error.h"

namespace eventide::language {

struct json_rpc_message {
    std::string jsonrpc = "2.0";
    std::string method;
};

std::string to_json(const json_rpc_message& message);
result<json_rpc_message> from_json(std::string_view payload);

}  // namespace eventide::language
