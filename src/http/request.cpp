#include "kota/http/client.h"
#include "kota/http/detail/client_state.h"
#include "kota/http/detail/runtime.h"
#include "kota/http/detail/util.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace kota::http {

namespace {

request make_request(method verb, std::string url, const client_options& options) {
    request req;
    req.verb = verb;
    req.url = std::move(url);
    req.headers = options.default_headers;
    req.cookie = options.default_cookie;
    req.user_agent = options.user_agent;
    req.proxy_config = options.proxy_config;
    req.redirect = options.redirect;
    req.tls = options.tls;
    req.timeout = options.timeout;
    req.use_cookie_store = options.use_cookie_store;
    req.disable_proxy = options.disable_proxy;
    return req;
}

task<response, error> make_failed_response(error err) {
    co_await fail(std::move(err));
}

template <typename Fn>
auto with_cookie_easy(const client_state& state, Fn&& fn) {
    curl::easy_handle easy = curl::easy_handle::create();
    using result_t = decltype(fn(easy.get()));
    if(!easy || !state.bind_easy(easy.get(), true)) {
        if constexpr(std::is_void_v<result_t>) {
            return;
        } else {
            return result_t{};
        }
    }

    return fn(easy.get());
}

void require_share_setopt(CURLSH* share,
                          CURLSHoption option,
                          auto value,
                          const char* message) noexcept {
    [[maybe_unused]] auto err = curl::share_setopt(share, option, value);
    assert(curl::ok(err) && message);
}

}  // namespace

client_state::client_state(client_options opts) :
    defaults(std::move(opts)), share(curl::share_handle::create()) {
    if(auto code = detail::ensure_curl_runtime(); !curl::ok(code)) {
        std::abort();
    }

    if(!share) {
        std::abort();
    }

    require_share_setopt(
        share.get(), CURLSHOPT_LOCKFUNC, &client_state::on_share_lock, "curl share lock registration failed");
    require_share_setopt(
        share.get(), CURLSHOPT_UNLOCKFUNC, &client_state::on_share_unlock, "curl share unlock registration failed");
    require_share_setopt(
        share.get(), CURLSHOPT_USERDATA, static_cast<void*>(this), "curl share userdata registration failed");
    require_share_setopt(
        share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE, "curl share cookie registration failed");
    require_share_setopt(
        share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS, "curl share dns registration failed");
    require_share_setopt(
        share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION, "curl share ssl session registration failed");
}

void client_state::bind(event_loop& loop) noexcept {
    bound_loop = &loop;
}

bool client_state::is_bound() const noexcept {
    return bound_loop != nullptr;
}

std::optional<std::reference_wrapper<event_loop>> client_state::loop() const noexcept {
    if(!bound_loop) {
        return std::nullopt;
    }

    return *bound_loop;
}

const client_options& client_state::options() const noexcept {
    return defaults;
}

bool client_state::bind_easy(CURL* easy, bool enable_cookie_store) const noexcept {
    if(!easy || !share) {
        return false;
    }

    if(auto err = curl::setopt(easy, CURLOPT_SHARE, share.get()); !curl::ok(err)) {
        return false;
    }

    if(enable_cookie_store) {
        if(auto err = curl::setopt(easy, CURLOPT_COOKIEFILE, ""); !curl::ok(err)) {
            return false;
        }
    }

    return true;
}

void client_state::clear_cookies() {
    with_cookie_easy(*this, [](CURL* easy) {
        [[maybe_unused]] auto err = curl::setopt(easy, CURLOPT_COOKIELIST, "ALL");
    });
}

void client_state::store_cookie(std::string_view url, std::string_view value) {
    if(value.empty()) {
        return;
    }

    with_cookie_easy(*this, [&](CURL* easy) {
        std::string url_text(url);
        std::string value_text(value);
        if(!url.empty()) {
            [[maybe_unused]] auto url_err = curl::setopt(easy, CURLOPT_URL, url_text.c_str());
        }
        [[maybe_unused]] auto err = curl::setopt(easy, CURLOPT_COOKIELIST, value_text.c_str());
    });
}

std::vector<std::string> client_state::cookie_list() const {
    return with_cookie_easy(*this, [&](CURL* easy) {
        curl_slist* raw = nullptr;
        if(auto err = curl::getinfo(easy, CURLINFO_COOKIELIST, &raw); !curl::ok(err) || !raw) {
            return std::vector<std::string>{};
        }

        curl::slist owned(raw);
        std::vector<std::string> out;
        for(auto* it = raw; it != nullptr; it = it->next) {
            if(!it->data) {
                continue;
            }
            out.emplace_back(it->data);
        }
        return out;
    });
}

void client_state::on_share_lock(CURL*, curl_lock_data data, curl_lock_access, void* userptr) noexcept {
    auto* self = static_cast<client_state*>(userptr);
    if(!self) {
        return;
    }
    self->mutex_for(data).lock();
}

void client_state::on_share_unlock(CURL*, curl_lock_data data, void* userptr) noexcept {
    auto* self = static_cast<client_state*>(userptr);
    if(!self) {
        return;
    }
    self->mutex_for(data).unlock();
}

std::mutex& client_state::mutex_for(curl_lock_data data) noexcept {
    switch(data) {
        case CURL_LOCK_DATA_COOKIE: return cookie_mu;
        case CURL_LOCK_DATA_DNS: return dns_mu;
        case CURL_LOCK_DATA_SSL_SESSION: return ssl_session_mu;
        default: return admin_mu;
    }
}

request_builder::request_builder(client_state* owner, event_loop* dispatch_loop, request req) noexcept :
    owner(owner), dispatch_loop(dispatch_loop), spec(std::move(req)) {}

request_builder& request_builder::header(std::string name, std::string value) {
    detail::upsert_header(spec.headers, std::move(name), std::move(value));
    return *this;
}

request_builder& request_builder::query(std::string name, std::string value) {
    spec.query.push_back({std::move(name), std::move(value)});
    return *this;
}

request_builder& request_builder::cookie(std::string value) {
    spec.cookie = std::move(value);
    return *this;
}

request_builder& request_builder::bearer_auth(std::string token) {
    return header("authorization", "Bearer " + token);
}

request_builder& request_builder::basic_auth(std::string username, std::string password) {
    return header("authorization", "Basic " + detail::base64_encode(username + ":" + password));
}

request_builder& request_builder::user_agent(std::string value) {
    spec.user_agent = std::move(value);
    return *this;
}

request_builder& request_builder::proxy(http::proxy value) {
    spec.proxy_config = std::move(value);
    spec.disable_proxy = false;
    return *this;
}

request_builder& request_builder::proxy(std::string url) {
    return proxy(http::proxy{.url = std::move(url), .username = {}, .password = {}});
}

request_builder& request_builder::no_proxy() {
    spec.proxy_config.reset();
    spec.disable_proxy = true;
    return *this;
}

request_builder& request_builder::timeout(std::chrono::milliseconds value) {
    spec.timeout = value;
    return *this;
}

request_builder& request_builder::no_cookies() {
    spec.use_cookie_store = false;
    return *this;
}

request_builder& request_builder::json_text(std::string body) {
    spec.body = std::move(body);
    detail::upsert_header(spec.headers, "content-type", "application/json");
    return *this;
}

request_builder& request_builder::form(std::vector<query_param> fields) {
    spec.body = detail::encode_pairs(fields);
    detail::upsert_header(spec.headers, "content-type", "application/x-www-form-urlencoded");
    return *this;
}

request_builder& request_builder::body(std::string body) {
    spec.body = std::move(body);
    return *this;
}

task<response, error> request_builder::failed(error err) {
    return make_failed_response(std::move(err));
}

std::optional<std::reference_wrapper<event_loop>>
request_builder::resolve_loop(client_state* owner, event_loop* dispatch_loop) noexcept {
    if(dispatch_loop) {
        return *dispatch_loop;
    }

    if(!owner) {
        return std::nullopt;
    }

    return owner->loop();
}

client_builder::client_builder(event_loop& loop) noexcept : bound_loop(&loop) {}

client_builder& client_builder::bind(event_loop& loop) noexcept {
    bound_loop = &loop;
    return *this;
}

client_builder& client_builder::default_header(std::string name, std::string value) {
    detail::upsert_header(options.default_headers, std::move(name), std::move(value));
    return *this;
}

client_builder& client_builder::default_cookie(std::string value) {
    options.default_cookie = std::move(value);
    return *this;
}

client_builder& client_builder::user_agent(std::string value) {
    options.user_agent = std::move(value);
    return *this;
}

client_builder& client_builder::proxy(http::proxy value) {
    options.proxy_config = std::move(value);
    options.disable_proxy = false;
    return *this;
}

client_builder& client_builder::proxy(std::string url) {
    return proxy(http::proxy{.url = std::move(url), .username = {}, .password = {}});
}

client_builder& client_builder::no_proxy() {
    options.proxy_config.reset();
    options.disable_proxy = true;
    return *this;
}

client_builder& client_builder::timeout(std::chrono::milliseconds value) {
    options.timeout = value;
    return *this;
}

client_builder& client_builder::cookie_store(bool enabled) {
    options.use_cookie_store = enabled;
    return *this;
}

client_builder& client_builder::redirect(redirect_policy value) {
    options.redirect = value;
    return *this;
}

client_builder& client_builder::referer(bool enabled) {
    options.redirect.referer = enabled;
    return *this;
}

client_builder& client_builder::https_only(bool enabled) {
    options.tls.https_only = enabled;
    return *this;
}

client_builder& client_builder::danger_accept_invalid_certs(bool enabled) {
    options.tls.danger_accept_invalid_certs = enabled;
    return *this;
}

client_builder& client_builder::danger_accept_invalid_hostnames(bool enabled) {
    options.tls.danger_accept_invalid_hostnames = enabled;
    return *this;
}

client_builder& client_builder::min_tls_version(http::tls_version value) {
    options.tls.min_version = value;
    return *this;
}

client_builder& client_builder::max_tls_version(http::tls_version value) {
    options.tls.max_version = value;
    return *this;
}

client_builder& client_builder::ca_file(std::string path) {
    options.tls.ca_file = std::move(path);
    return *this;
}

client_builder& client_builder::ca_path(std::string path) {
    options.tls.ca_path = std::move(path);
    return *this;
}

std::expected<client, error> client_builder::build() const& {
    if(auto code = detail::ensure_curl_runtime(); !curl::ok(code)) {
        return std::unexpected(error::from_curl(code));
    }

    client out(options);
    if(bound_loop) {
        out.bind(*bound_loop);
    }
    return out;
}

std::expected<client, error> client_builder::build() && {
    if(auto code = detail::ensure_curl_runtime(); !curl::ok(code)) {
        return std::unexpected(error::from_curl(code));
    }

    client out(std::move(options));
    if(bound_loop) {
        out.bind(*bound_loop);
    }
    return out;
}

client::client(client_options options) :
    state(std::make_unique<client_state>(std::move(options))) {}

client::client(event_loop& loop, client_options options) : client(std::move(options)) {
    bind(loop);
}

client::~client() = default;

client_builder client::builder() noexcept {
    return client_builder();
}

client_builder client::builder(event_loop& loop) noexcept {
    return client_builder(loop);
}

client::client(client&&) noexcept = default;

client& client::operator=(client&&) noexcept = default;

client& client::bind(event_loop& loop) noexcept {
    state->bind(loop);
    return *this;
}

bool client::is_bound() const noexcept {
    return state->is_bound();
}

bound_client client::on(event_loop& loop) & noexcept {
    return bound_client(state.get(), loop);
}

request_builder client::request(method verb, std::string url) const& {
    return request_builder(state.get(), nullptr, make_request(verb, std::move(url), state->options()));
}

request_builder client::get(std::string url) const& {
    return request(method::get, std::move(url));
}

request_builder client::post(std::string url) const& {
    return request(method::post, std::move(url));
}

request_builder client::put(std::string url) const& {
    return request(method::put, std::move(url));
}

request_builder client::patch(std::string url) const& {
    return request(method::patch, std::move(url));
}

request_builder client::del(std::string url) const& {
    return request(method::del, std::move(url));
}

request_builder client::head(std::string url) const& {
    return request(method::head, std::move(url));
}

task<response, error> client::execute(http::request req) const& {
    auto bound = loop();
    if(!bound) {
        return make_failed_response(
            error::invalid_request("client::execute requires a bound loop"));
    }
    return detail::execute_with_state(std::move(req), bound->get(), state.get());
}

client& client::store_cookie(std::string url, std::string value) {
    state->store_cookie(url, std::move(value));
    return *this;
}

std::vector<std::string> client::cookie_list() const {
    return state->cookie_list();
}

void client::clear_cookies() {
    state->clear_cookies();
}

std::optional<std::reference_wrapper<event_loop>> client::loop() const noexcept {
    return state->loop();
}

const client_options& client::options() const noexcept {
    return state->options();
}

request_builder bound_client::request(method verb, std::string url) const noexcept {
    return request_builder(state, dispatch_loop, make_request(verb, std::move(url), state->options()));
}

request_builder bound_client::get(std::string url) const noexcept {
    return request(method::get, std::move(url));
}

request_builder bound_client::post(std::string url) const noexcept {
    return request(method::post, std::move(url));
}

request_builder bound_client::put(std::string url) const noexcept {
    return request(method::put, std::move(url));
}

request_builder bound_client::patch(std::string url) const noexcept {
    return request(method::patch, std::move(url));
}

request_builder bound_client::del(std::string url) const noexcept {
    return request(method::del, std::move(url));
}

request_builder bound_client::head(std::string url) const noexcept {
    return request(method::head, std::move(url));
}

task<response, error> bound_client::execute(http::request req) const {
    if(!state || !dispatch_loop) {
        return make_failed_response(error::invalid_request("bound_client requires a valid loop"));
    }

    return detail::execute_with_state(std::move(req), *dispatch_loop, state);
}

}  // namespace kota::http
