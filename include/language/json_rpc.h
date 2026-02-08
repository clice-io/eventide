#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "eventide/error.h"
#include "language/json_value.h"

#include <rfl/Rename.hpp>
#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>

#if __has_include(<yyjson.h>)
#include <yyjson.h>
#else
#include <rfl/thirdparty/yyjson.h>
#endif

namespace eventide::language::json_rpc {

using id = std::variant<std::int64_t, std::string>;

struct message {
    std::string method;
    std::optional<id> request_id;
    yyjson_val* params = nullptr;
    std::shared_ptr<yyjson_doc> doc;
};

result<message> parse(std::string_view payload);

template <typename Params>
result<Params> parse_params(const message& msg) {
    if(msg.params == nullptr || yyjson_is_null(msg.params)) {
        if constexpr(std::is_default_constructible_v<Params>) {
            return Params{};
        } else {
            return std::unexpected(error::invalid_argument);
        }
    }

    auto res = rfl::json::read<Params>(rfl::json::InputVarType(msg.params));
    if(!res) {
        return std::unexpected(error::invalid_argument);
    }

    return std::move(res.value());
}

template <typename Result>
std::string make_result(const id& request_id, const Result& value) {
    struct response_message {
        std::string jsonrpc = "2.0";
        rfl::Rename<"id", id> request_id;
        Result result;
    } response{std::string("2.0"), request_id, value};

    return rfl::json::write(response);
}

struct error_object {
    std::int32_t code = 0;
    std::string message;
    std::optional<json_value> data;
};

inline std::string make_error(const id& request_id, const error_object& error) {
    struct error_response {
        std::string jsonrpc = "2.0";
        rfl::Rename<"id", id> request_id;
        error_object error;
    } response{std::string("2.0"), request_id, error};

    return rfl::json::write(response);
}

}  // namespace eventide::language::json_rpc
