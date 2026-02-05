#include "language/json_rpc.h"

#include <string>

#include "rfl/json.hpp"

namespace eventide::language {

std::string to_json(const json_rpc_message& message) {
    return rfl::json::write(message);
}

result<json_rpc_message> from_json(std::string_view payload) {
    auto parsed = rfl::json::read<json_rpc_message>(std::string(payload));
    if(!parsed) {
        return std::unexpected(error::invalid_argument);
    }

    return parsed.value();
}

}  // namespace eventide::language
