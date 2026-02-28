#pragma once

#ifndef EVENTIDE_LANGUAGE_SERVER_INL_FROM_HEADER
#include "eventide/language/server.h"
#endif

#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "eventide/common/function_traits.h"

namespace eventide::language {

namespace detail {

template <typename Params>
constexpr bool has_request_traits_v = requires {
    typename protocol::RequestTraits<Params>::Result;
    protocol::RequestTraits<Params>::method;
};

template <typename Params>
constexpr bool has_notification_traits_v = requires {
    protocol::NotificationTraits<Params>::method;
};

template <typename Callback>
using request_callback_args_t = callable_args_t<std::remove_cvref_t<Callback>>;

template <typename Callback>
using request_callback_params_t =
    std::remove_cvref_t<std::tuple_element_t<1, request_callback_args_t<Callback>>>;

template <typename Callback>
using request_callback_return_t = callable_return_t<std::remove_cvref_t<Callback>>;

template <typename Callback>
using notification_callback_args_t = callable_args_t<std::remove_cvref_t<Callback>>;

template <typename Callback>
using notification_callback_params_t =
    std::remove_cvref_t<std::tuple_element_t<0, notification_callback_args_t<Callback>>>;

template <typename Callback>
using notification_callback_return_t = callable_return_t<std::remove_cvref_t<Callback>>;

template <typename Callback>
consteval void validate_request_callback_signature() {
    using Args = request_callback_args_t<Callback>;
    static_assert(std::tuple_size_v<Args> == 2, "request callback should have two parameters");

    using Context = std::remove_cvref_t<std::tuple_element_t<0, Args>>;
    static_assert(std::is_same_v<Context, RequestContext>,
                  "request callback first parameter should be RequestContext");
}

template <typename Callback>
consteval void validate_notification_callback_signature() {
    using Args = notification_callback_args_t<Callback>;
    static_assert(std::tuple_size_v<Args> == 1,
                  "notification callback should have one parameter");

    using Ret = notification_callback_return_t<Callback>;
    static_assert(std::is_same_v<Ret, void>, "notification callback should return void");
}

}  // namespace detail

template <typename Params>
RequestResult<Params> LanguageServer::send_request(const Params& params) {
    static_assert(detail::has_request_traits_v<Params>,
                  "send_request(params) requires RequestTraits<Params>");
    using Traits = protocol::RequestTraits<Params>;

    co_return co_await send_request<typename Traits::Result>(Traits::method, params);
}

template <typename Result, typename Params>
task<std::expected<Result, std::string>> LanguageServer::send_request(std::string_view method,
                                                                      const Params& params) {
    co_return co_await jsonrpc::Peer::send_request<Result>(method, params);
}

template <typename Params>
std::expected<void, std::string> LanguageServer::send_notification(const Params& params) {
    static_assert(detail::has_notification_traits_v<Params>,
                  "send_notification(params) requires NotificationTraits<Params>");
    using Traits = protocol::NotificationTraits<Params>;

    return send_notification(Traits::method, params);
}

template <typename Params>
std::expected<void, std::string> LanguageServer::send_notification(std::string_view method,
                                                                   const Params& params) {
    return jsonrpc::Peer::send_notification(method, params);
}

template <typename Callback>
void LanguageServer::on_request(Callback&& callback) {
    detail::validate_request_callback_signature<Callback>();

    using Params = detail::request_callback_params_t<Callback>;
    static_assert(detail::has_request_traits_v<Params>,
                  "on_request(callback) requires RequestTraits<Params>");

    using Ret = detail::request_callback_return_t<Callback>;
    static_assert(std::is_same_v<Ret, RequestResult<Params>>,
                  "request callback return type should be RequestResult<Params>");

    on_request(protocol::RequestTraits<Params>::method, std::forward<Callback>(callback));
}

template <typename Callback>
void LanguageServer::on_request(std::string_view method, Callback&& callback) {
    detail::validate_request_callback_signature<Callback>();

    using Params = detail::request_callback_params_t<Callback>;
    jsonrpc::Peer::on_request(
        method,
        [cb = std::forward<Callback>(callback), server = this](jsonrpc::RequestContext& context,
                                                               const Params& params)
            -> detail::request_callback_return_t<Callback> {
            RequestContext lsp_context(*server, context.id);
            lsp_context.method = context.method;
            co_return co_await std::invoke(cb, lsp_context, params);
        });
}

template <typename Callback>
void LanguageServer::on_notification(Callback&& callback) {
    detail::validate_notification_callback_signature<Callback>();

    using Params = detail::notification_callback_params_t<Callback>;
    static_assert(detail::has_notification_traits_v<Params>,
                  "on_notification(callback) requires NotificationTraits<Params>");

    on_notification(protocol::NotificationTraits<Params>::method, std::forward<Callback>(callback));
}

template <typename Callback>
void LanguageServer::on_notification(std::string_view method, Callback&& callback) {
    detail::validate_notification_callback_signature<Callback>();
    jsonrpc::Peer::on_notification(method, std::forward<Callback>(callback));
}

}  // namespace eventide::language
