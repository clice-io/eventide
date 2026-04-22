#pragma once

#include <chrono>
#include <concepts>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "kota/http/detail/options.h"
#include "kota/http/detail/request.h"
#include "kota/http/detail/response.h"
#include "kota/async/io/loop.h"
#include "kota/async/runtime/task.h"

#if __has_include(<simdjson.h>)
#include "kota/codec/json.h"
#define KOTA_HTTP_HAS_CODEC_JSON 1
#else
#define KOTA_HTTP_HAS_CODEC_JSON 0
#endif

namespace kota::http {

struct client_state;
class client_builder;
class bound_client;

namespace detail {

task<response, error> execute_with_state(request req,
                                         event_loop& loop,
                                         std::shared_ptr<client_state> owner);

template <typename T>
curl_option_hook make_curl_option(CURLoption option, T&& value) {
    using stored_t = std::decay_t<T>;
    static_assert(std::is_copy_constructible_v<stored_t>,
                  "native curl option values must be copy constructible");

    if constexpr(std::same_as<stored_t, std::string>) {
        return [option, value = std::move(value)](CURL* easy) -> curl::easy_error {
            return curl::setopt(easy, option, value.c_str());
        };
    } else if constexpr(std::same_as<stored_t, std::string_view>) {
        std::string owned(value);
        return [option, owned = std::move(owned)](CURL* easy) -> curl::easy_error {
            return curl::setopt(easy, option, owned.c_str());
        };
    } else {
        return [option, value = std::forward<T>(value)](CURL* easy) -> curl::easy_error {
            return curl::setopt(easy, option, value);
        };
    }
}

}  // namespace detail

class request_builder {
public:
    request_builder(std::shared_ptr<client_state> owner,
                    event_loop* dispatch_loop,
                    request req) noexcept;

    // Upsert a request header. Names are matched case-insensitively.
    request_builder& header(std::string name, std::string value);

    // Append a query parameter. Repeated keys are preserved in insertion order.
    request_builder& query(std::string name, std::string value);

    // Set the raw Cookie header for this request.
    request_builder& cookies(std::string value);

    // Override the HTTP method string, for example "OPTIONS" or "PROPFIND".
    request_builder& method(std::string value);

    request_builder& bearer_auth(std::string token);

    request_builder& basic_auth(std::string username, std::string password);

    request_builder& user_agent(std::string value);

    request_builder& proxy(http::proxy value);

    request_builder& proxy(std::string url);

    request_builder& no_proxy();

    request_builder& timeout(std::chrono::milliseconds value);

    // Apply a native libcurl option to this request.
    // The value is captured into an internal callable and applied right before dispatch.
    // Type validation is intentionally left to the caller: the provided value must match
    // the selected CURLOPT_* contract.
    template <typename T>
    request_builder& curl_option(CURLoption option, T&& value) {
        spec.curl_options.push_back(detail::make_curl_option(option, std::forward<T>(value)));
        return *this;
    }

    // Set the request body and force Content-Type: application/json.
    request_builder& json_text(std::string body);

    // Encode fields as application/x-www-form-urlencoded and set the matching Content-Type.
    request_builder& form(std::vector<query_param> fields);

    // Set the raw request body without touching Content-Type.
    request_builder& body(std::string body);

    // Materialize the current request snapshot without sending it.
    request build(this auto&& self) {
        if constexpr(std::is_rvalue_reference_v<decltype(self)>) {
            return std::move(self.spec);
        } else {
            return self.spec;
        }
    }

    // Send the request on the bound loop.
    // Any staged builder error (for example JSON encoding failure) is surfaced here as a
    // failed task before the request reaches libcurl.
    task<response, error> send(this auto&& self)
        requires (!std::is_const_v<std::remove_reference_t<decltype(self)>>) {
        auto pending_error = [&]() -> std::optional<error> {
            if(!self.staged_error) {
                return std::nullopt;
            }

            if constexpr(std::is_rvalue_reference_v<decltype(self)>) {
                return std::exchange(self.staged_error, std::nullopt);
            } else {
                return self.staged_error;
            }
        }();
        if(pending_error) {
            return failed(std::move(*pending_error));
        }

        if(!self.owner) {
            return failed(
                error::invalid_request("request_builder::send requires an owning client"));
        }

        auto loop = resolve_loop(self.owner, self.dispatch_loop);
        if(!loop) {
            return failed(error::invalid_request(
                "request_builder::send requires a loop via client::bind or client.on"));
        }

        auto owner = [&]() -> std::shared_ptr<client_state> {
            if constexpr(std::is_rvalue_reference_v<decltype(self)>) {
                return std::move(self.owner);
            } else {
                return self.owner;
            }
        }();

        return detail::execute_with_state(std::forward<decltype(self)>(self).build(),
                                          loop->get(),
                                          std::move(owner));
    }

#if KOTA_HTTP_HAS_CODEC_JSON
    template <typename T>
    // Serialize a value as JSON, set the body and Content-Type, and keep chaining.
    // Encoding failures are remembered on the builder and returned by send().
    request_builder& json(const T& value) {
        auto encoded = codec::json::to_string(value);
        if(!encoded) {
            remember_error(error::json_encode(encoded.error().to_string()));
            return *this;
        }

        json_text(std::move(*encoded));
        return *this;
    }
#endif

private:
    static task<response, error> failed(error err);
    static std::optional<std::reference_wrapper<event_loop>>
        resolve_loop(const std::shared_ptr<client_state>& owner,
                     event_loop* dispatch_loop) noexcept;
    void remember_error(error err) noexcept;

    std::shared_ptr<client_state> owner;
    [[maybe_unused]] event_loop* dispatch_loop = nullptr;
    request spec{};
    std::optional<error> staged_error;
};

class client {
public:
    explicit client(client_options options = {});
    client(event_loop& loop, client_options options = {});
    ~client();

    static client_builder builder() noexcept;
    static client_builder builder(event_loop& loop) noexcept;

    client(const client&) = delete;
    client& operator=(const client&) = delete;

    client(client&&) noexcept;
    client& operator=(client&&) noexcept;

    // Attach a default dispatch loop used by send() / execute().
    client& bind(event_loop& loop) noexcept;

    bool is_bound() const noexcept;

    // Create a lightweight loop-bound view without rebinding the client itself.
    bound_client on(event_loop& loop) & noexcept;

    // Start a request with an arbitrary HTTP method string.
    request_builder request(std::string method, std::string url) const&;
    request_builder request(std::string method, std::string url) && = delete;

    request_builder get(std::string url) const&;
    request_builder get(std::string url) && = delete;

    request_builder post(std::string url) const&;
    request_builder post(std::string url) && = delete;

    request_builder put(std::string url) const&;
    request_builder put(std::string url) && = delete;

    request_builder patch(std::string url) const&;
    request_builder patch(std::string url) && = delete;

    request_builder del(std::string url) const&;
    request_builder del(std::string url) && = delete;

    request_builder head(std::string url) const&;
    request_builder head(std::string url) && = delete;

    // Dispatch a fully materialized request using the client's bound loop.
    task<response, error> execute(http::request req) const&;
    task<response, error> execute(http::request req) && = delete;

    // Enable or disable automatic cookie handling for future requests created by this
    // client. When disabled, requests neither record Set-Cookie nor use the shared
    // cookie jar.
    client& record_cookie(bool enabled = true) noexcept;

    std::optional<std::reference_wrapper<event_loop>> loop() const noexcept;

    const client_options& options() const noexcept;

private:
    std::shared_ptr<client_state> state;
};

class bound_client {
public:
    bound_client(std::shared_ptr<client_state> state, event_loop& loop) noexcept :
        state(std::move(state)), dispatch_loop(&loop) {}

    // Start a request with an arbitrary HTTP method string on this view's loop.
    request_builder request(std::string method, std::string url) const noexcept;

    request_builder get(std::string url) const noexcept;

    request_builder post(std::string url) const noexcept;

    request_builder put(std::string url) const noexcept;

    request_builder patch(std::string url) const noexcept;

    request_builder del(std::string url) const noexcept;

    request_builder head(std::string url) const noexcept;

    // Dispatch a fully materialized request on this view's loop.
    task<response, error> execute(http::request req) const;

    event_loop& loop() const noexcept {
        return *dispatch_loop;
    }

private:
    std::shared_ptr<client_state> state;
    event_loop* dispatch_loop = nullptr;
};

class client_builder {
public:
    client_builder() noexcept = default;

    explicit client_builder(event_loop& loop) noexcept;

    // Set the loop that build() will bind the resulting client to.
    client_builder& bind(event_loop& loop) noexcept;

    // Add or replace a default header copied into every new request builder.
    client_builder& default_header(std::string name, std::string value);

    client_builder& user_agent(std::string value);

    client_builder& proxy(http::proxy value);

    client_builder& proxy(std::string url);

    client_builder& no_proxy();

    client_builder& timeout(std::chrono::milliseconds value);

    // Apply a native libcurl option to every request created by the built client.
    // Per-request curl_option() calls are applied later and may override these defaults.
    template <typename T>
    client_builder& curl_option(CURLoption option, T&& value) {
        options.curl_options.push_back(detail::make_curl_option(option, std::forward<T>(value)));
        return *this;
    }

    client_builder& redirect(redirect_policy value);

    client_builder& referer(bool enabled);

    client_builder& https_only(bool enabled = true);

    client_builder& danger_accept_invalid_certs(bool enabled = true);

    client_builder& danger_accept_invalid_hostnames(bool enabled = true);

    client_builder& min_tls_version(http::tls_version value);

    client_builder& max_tls_version(http::tls_version value);

    client_builder& ca_file(std::string path);

    client_builder& ca_path(std::string path);

    // Construct a client. Runtime initialization failures are reported in the expected.
    std::expected<client, error> build() const&;

    std::expected<client, error> build() &&;

private:
    event_loop* bound_loop = nullptr;
    client_options options{};
};

}  // namespace kota::http
