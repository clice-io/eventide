#pragma once

#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "kota/http/options.h"
#include "kota/http/request.h"
#include "kota/http/response.h"
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

task<response, error> execute_with_state(request req, event_loop& loop, client_state* owner);

}  // namespace detail

class request_builder {
public:
    request_builder(client_state* owner, event_loop* dispatch_loop, request req) noexcept;

    request_builder& header(std::string name, std::string value);

    request_builder& query(std::string name, std::string value);

    request_builder& cookie(std::string value);

    request_builder& bearer_auth(std::string token);

    request_builder& basic_auth(std::string username, std::string password);

    request_builder& user_agent(std::string value);

    request_builder& proxy(http::proxy value);

    request_builder& proxy(std::string url);

    request_builder& no_proxy();

    request_builder& timeout(std::chrono::milliseconds value);

    request_builder& no_cookies();

    request_builder& json_text(std::string body);

    request_builder& form(std::vector<query_param> fields);

    request_builder& body(std::string body);

    request build(this auto&& self) {
        if constexpr(std::is_rvalue_reference_v<decltype(self)>) {
            return std::move(self.spec);
        } else {
            return self.spec;
        }
    }

    task<response, error> send(this auto&& self)
        requires (!std::is_const_v<std::remove_reference_t<decltype(self)>>) {
        if(!self.owner) {
            return failed(
                error::invalid_request("request_builder::send requires an owning client"));
        }

        auto loop = resolve_loop(self.owner, self.dispatch_loop);
        if(!loop) {
            return failed(error::invalid_request(
                "request_builder::send requires a loop via client::bind or client.on"));
        }

        return detail::execute_with_state(std::forward<decltype(self)>(self).build(),
                                          loop->get(),
                                          self.owner);
    }

#if KOTA_HTTP_HAS_CODEC_JSON
    template <typename T>
    std::expected<void, error> json(const T& value) {
        auto encoded = codec::json::to_string(value);
        if(!encoded) {
            return std::unexpected(error::json_encode(encoded.error().to_string()));
        }

        json_text(std::move(*encoded));
        return {};
    }
#endif

private:
    static task<response, error> failed(error err);
    static std::optional<std::reference_wrapper<event_loop>>
        resolve_loop(client_state* owner, event_loop* dispatch_loop) noexcept;

    client_state* owner = nullptr;
    event_loop* dispatch_loop = nullptr;
    request spec{};
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

    client& bind(event_loop& loop) noexcept;

    bool is_bound() const noexcept;

    bound_client on(event_loop& loop) & noexcept;

    request_builder request(method verb, std::string url) const&;
    request_builder request(method verb, std::string url) && = delete;

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

    task<response, error> execute(http::request req) const&;
    task<response, error> execute(http::request req) && = delete;

    client& store_cookie(std::string url, std::string value);

    std::vector<std::string> cookie_list() const;

    void clear_cookies();

    std::optional<std::reference_wrapper<event_loop>> loop() const noexcept;

    const client_options& options() const noexcept;

private:
    std::unique_ptr<client_state> state;
};

class bound_client {
public:
    bound_client(client_state* state, event_loop& loop) noexcept :
        state(state), dispatch_loop(&loop) {}

    request_builder request(method verb, std::string url) const noexcept;

    request_builder get(std::string url) const noexcept;

    request_builder post(std::string url) const noexcept;

    request_builder put(std::string url) const noexcept;

    request_builder patch(std::string url) const noexcept;

    request_builder del(std::string url) const noexcept;

    request_builder head(std::string url) const noexcept;

    task<response, error> execute(http::request req) const;

    event_loop& loop() const noexcept {
        return *dispatch_loop;
    }

    client_state* state = nullptr;
    event_loop* dispatch_loop = nullptr;
};

class client_builder {
public:
    client_builder() noexcept = default;

    explicit client_builder(event_loop& loop) noexcept;

    client_builder& bind(event_loop& loop) noexcept;

    client_builder& default_header(std::string name, std::string value);

    client_builder& default_cookie(std::string value);

    client_builder& user_agent(std::string value);

    client_builder& proxy(http::proxy value);

    client_builder& proxy(std::string url);

    client_builder& no_proxy();

    client_builder& timeout(std::chrono::milliseconds value);

    client_builder& cookie_store(bool enabled = true);

    client_builder& redirect(redirect_policy value);

    client_builder& referer(bool enabled);

    client_builder& https_only(bool enabled = true);

    client_builder& danger_accept_invalid_certs(bool enabled = true);

    client_builder& danger_accept_invalid_hostnames(bool enabled = true);

    client_builder& min_tls_version(http::tls_version value);

    client_builder& max_tls_version(http::tls_version value);

    client_builder& ca_file(std::string path);

    client_builder& ca_path(std::string path);

    std::expected<client, error> build() const&;

    std::expected<client, error> build() &&;

    event_loop* bound_loop = nullptr;
    client_options options{};
};

}  // namespace kota::http
