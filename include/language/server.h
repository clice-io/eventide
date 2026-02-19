#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "eventide/task.h"
#include "language/protocol.h"
#include "language/transport.h"
#include "reflection/function.h"
#include "serde/simdjson/deserializer.h"
#include "serde/simdjson/serializer.h"

namespace language {

namespace et = eventide;

struct RequestContext {
    std::string_view method{};
};

template <typename Params>
using RequestResult =
    et::task<std::expected<typename protocol::RequestTraits<Params>::Result, std::string>>;

class LanguageServer {
public:
    LanguageServer();

    explicit LanguageServer(std::unique_ptr<Transport> transport);

    LanguageServer(const LanguageServer&) = delete;
    LanguageServer& operator=(const LanguageServer&) = delete;
    LanguageServer(LanguageServer&&) = delete;
    LanguageServer& operator=(LanguageServer&&) = delete;

    ~LanguageServer();

    int start();

    template <typename Callback>
    void on_request(Callback&& callback) {
        using F = std::remove_cvref_t<Callback>;
        using Args = refl::callable_args_t<F>;
        static_assert(std::tuple_size_v<Args> == 2, "request callback should have two parameters");

        using Context = std::remove_cvref_t<std::tuple_element_t<0, Args>>;
        using Params = std::remove_cvref_t<std::tuple_element_t<1, Args>>;
        using Traits = protocol::RequestTraits<Params>;

        static_assert(std::is_same_v<Context, RequestContext>,
                      "request callback first parameter should be RequestContext");
        static_assert(requires {
            typename Traits::Result;
            Traits::method;
        });

        using Ret = refl::callable_return_t<F>;
        static_assert(request_return_traits<Ret>::valid,
                      "request callback return type should be task<expected<Result, std::string>>");
        using Result = typename request_return_traits<Ret>::result_type;
        static_assert(std::is_same_v<Result, typename Traits::Result>,
                      "request callback result type should match RequestTraits<Params>::Result");

        on_request(Traits::method, std::forward<Callback>(callback));
    }

    template <typename Callback>
    void on_request(std::string_view method, Callback&& callback) {
        using F = std::remove_cvref_t<Callback>;
        using Args = refl::callable_args_t<F>;
        static_assert(std::tuple_size_v<Args> == 2, "request callback should have two parameters");

        using Context = std::remove_cvref_t<std::tuple_element_t<0, Args>>;
        using Params = std::remove_cvref_t<std::tuple_element_t<1, Args>>;

        static_assert(std::is_same_v<Context, RequestContext>,
                      "request callback first parameter should be RequestContext");

        using Ret = refl::callable_return_t<F>;
        static_assert(request_return_traits<Ret>::valid,
                      "request callback return type should be task<expected<Result, std::string>>");

        auto wrapped =
            [cb = std::forward<Callback>(callback), method_name = std::string(method)](
                std::string_view params_json) -> et::task<std::expected<std::string, std::string>> {
            auto parsed_params = LanguageServer::template deserialize_json<Params>(
                LanguageServer::template normalize_params_json<Params>(params_json));
            if(!parsed_params) {
                co_return std::unexpected(parsed_params.error());
            }

            RequestContext context{};
            context.method = method_name;

            auto result = co_await std::invoke(cb, context, *parsed_params);
            if(!result) {
                co_return std::unexpected(result.error());
            }

            auto serialized = LanguageServer::serialize_json(*result);
            if(!serialized) {
                co_return std::unexpected(serialized.error());
            }

            co_return std::move(*serialized);
        };

        register_request_handler(method, std::move(wrapped));
    }

    template <typename Callback>
    void on_notification(Callback&& callback) {
        using F = std::remove_cvref_t<Callback>;
        using Args = refl::callable_args_t<F>;
        static_assert(std::tuple_size_v<Args> == 1,
                      "notification callback should have one parameter");

        using Params = std::remove_cvref_t<std::tuple_element_t<0, Args>>;
        using Traits = protocol::NotificationTraits<Params>;
        static_assert(requires { Traits::method; });

        using Ret = refl::callable_return_t<F>;
        static_assert(std::is_same_v<Ret, void>, "notification callback should return void");

        on_notification(Traits::method, std::forward<Callback>(callback));
    }

    template <typename Callback>
    void on_notification(std::string_view method, Callback&& callback) {
        using F = std::remove_cvref_t<Callback>;
        using Args = refl::callable_args_t<F>;
        static_assert(std::tuple_size_v<Args> == 1,
                      "notification callback should have one parameter");

        using Params = std::remove_cvref_t<std::tuple_element_t<0, Args>>;
        using Ret = refl::callable_return_t<F>;
        static_assert(std::is_same_v<Ret, void>, "notification callback should return void");

        auto wrapped = [cb = std::forward<Callback>(callback)](std::string_view params_json) {
            auto parsed_params = LanguageServer::template deserialize_json<Params>(
                LanguageServer::template normalize_params_json<Params>(params_json));
            if(!parsed_params) {
                return;
            }
            std::invoke(cb, *parsed_params);
        };

        register_notification_handler(method, std::move(wrapped));
    }

private:
    using RequestHandler =
        std::function<et::task<std::expected<std::string, std::string>>(std::string_view)>;
    using NotificationHandler = std::function<void(std::string_view)>;

    template <typename T>
    struct request_return_traits {
        constexpr static bool valid = false;
    };

    template <typename Result>
    struct request_return_traits<et::task<std::expected<Result, std::string>>> {
        constexpr static bool valid = true;
        using result_type = Result;
    };

    template <typename Params>
    constexpr static std::string_view normalize_params_json(std::string_view params_json) {
        if(!params_json.empty()) {
            return params_json;
        }
        if constexpr(std::is_same_v<Params, protocol::null> ||
                     std::is_same_v<Params, protocol::LSPAny>) {
            return "null";
        }
        return "{}";
    }

    template <typename T>
    static std::expected<T, std::string> deserialize_json(std::string_view json) {
        auto parsed = serde::json::simd::from_json<T>(json);
        if(!parsed) {
            return std::unexpected(std::string(simdjson::error_message(parsed.error())));
        }
        return std::move(*parsed);
    }

    template <typename T>
    static std::expected<std::string, std::string> serialize_json(const T& value) {
        auto serialized = serde::json::simd::to_json(value);
        if(!serialized) {
            return std::unexpected(std::string(simdjson::error_message(serialized.error())));
        }
        return std::move(*serialized);
    }

    void register_request_handler(std::string_view method, RequestHandler handler);

    void register_notification_handler(std::string_view method, NotificationHandler handler);

private:
    struct Self;
    std::unique_ptr<Self> self;
};

}  // namespace language
