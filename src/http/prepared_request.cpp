#include "kota/http/detail/prepared_request.h"

#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>

#include "kota/http/detail/util.h"

namespace kota::http {

namespace {

constexpr int tls_version_rank(http::tls_version value) noexcept {
    switch(value) {
        case http::tls_version::tls1_0: return 10;
        case http::tls_version::tls1_1: return 11;
        case http::tls_version::tls1_2: return 12;
        case http::tls_version::tls1_3: return 13;
    }

    return 0;
}

}  // namespace

namespace detail {

long to_curl_ssl_min(http::tls_version value) noexcept {
    switch(value) {
        case http::tls_version::tls1_0: return CURL_SSLVERSION_TLSv1_0;
        case http::tls_version::tls1_1: return CURL_SSLVERSION_TLSv1_1;
        case http::tls_version::tls1_2: return CURL_SSLVERSION_TLSv1_2;
        case http::tls_version::tls1_3: return CURL_SSLVERSION_TLSv1_3;
    }

    return CURL_SSLVERSION_DEFAULT;
}

long to_curl_ssl_max(http::tls_version value) noexcept {
    switch(value) {
        case http::tls_version::tls1_0:
#ifdef CURL_SSLVERSION_MAX_TLSv1_0
            return CURL_SSLVERSION_MAX_TLSv1_0;
#else
            return 0;
#endif
        case http::tls_version::tls1_1:
#ifdef CURL_SSLVERSION_MAX_TLSv1_1
            return CURL_SSLVERSION_MAX_TLSv1_1;
#else
            return 0;
#endif
        case http::tls_version::tls1_2:
#ifdef CURL_SSLVERSION_MAX_TLSv1_2
            return CURL_SSLVERSION_MAX_TLSv1_2;
#else
            return 0;
#endif
        case http::tls_version::tls1_3:
#ifdef CURL_SSLVERSION_MAX_TLSv1_3
            return CURL_SSLVERSION_MAX_TLSv1_3;
#else
            return 0;
#endif
    }

    return 0;
}

std::size_t
    prepared_request::on_write(char* data, std::size_t size, std::size_t count, void* userdata) {
    auto* self = static_cast<prepared_request*>(userdata);
    assert(self != nullptr && "curl write callback requires prepared_request");

    const auto bytes = size * count;
    auto* begin = reinterpret_cast<const std::byte*>(data);
    self->out.body.insert(self->out.body.end(), begin, begin + bytes);
    return bytes;
}

std::size_t
    prepared_request::on_header(char* data, std::size_t size, std::size_t count, void* userdata) {
    auto* self = static_cast<prepared_request*>(userdata);
    assert(self != nullptr && "curl header callback requires prepared_request");

    const auto bytes = size * count;
    std::string_view line(data, bytes);
    while(line.ends_with('\n') || line.ends_with('\r')) {
        line.remove_suffix(1);
    }

    if(line.empty()) {
        return bytes;
    }

    if(line.starts_with("HTTP/")) {
        self->out.headers.clear();
        return bytes;
    }

    const auto colon = line.find(':');
    if(colon == std::string_view::npos) {
        return bytes;
    }

    auto name = detail::trim_ascii(line.substr(0, colon));
    auto value = detail::trim_ascii(line.substr(colon + 1));
    self->out.headers.push_back({name, value});

    return bytes;
}

bool prepared_request::fail(error err) noexcept {
    result = std::move(err);
    return false;
}

bool prepared_request::fail(curl::easy_error code) noexcept {
    return fail(error::from_curl(code));
}

bool prepared_request::apply_url() noexcept {
    if(spec.url.empty()) {
        return fail(error::invalid_request("request url must not be empty"));
    }

    final_url = spec.url;
    if(!spec.query.empty()) {
        final_url += final_url.find('?') != std::string::npos ? '&' : '?';
        final_url += detail::encode_pairs(spec.query);
    }

    return easy_setopt(*this, CURLOPT_URL, final_url.c_str());
}

bool prepared_request::apply_method() noexcept {
    if(spec.method.empty()) {
        return fail(error::invalid_request("request method must not be empty"));
    }

    if(detail::iequals(spec.method, http::method::get)) {
        return true;
    }

    if(detail::iequals(spec.method, http::method::post)) {
        return easy_setopt(*this, CURLOPT_POST, 1L);
    }

    if(detail::iequals(spec.method, http::method::head)) {
        return easy_setopt(*this, CURLOPT_NOBODY, 1L) &&
               easy_setopt(*this, CURLOPT_CUSTOMREQUEST, spec.method.c_str());
    }

    return easy_setopt(*this, CURLOPT_CUSTOMREQUEST, spec.method.c_str());
}

bool prepared_request::apply_body() noexcept {
    if(spec.body.empty()) {
        return true;
    }

    if(detail::iequals(spec.method, http::method::get) ||
       detail::iequals(spec.method, http::method::head)) {
        return fail(error::invalid_request("request body is not supported for GET or HEAD"));
    }

    return easy_setopt(*this,
                       CURLOPT_POSTFIELDSIZE_LARGE,
                       static_cast<curl_off_t>(spec.body.size())) &&
           easy_setopt(*this, CURLOPT_COPYPOSTFIELDS, spec.body.c_str());
}

bool prepared_request::apply_headers() noexcept {
    for(const auto& item: spec.headers) {
        std::string line = item.name;
        line += ": ";
        line += item.value;
        if(!header_lines.append(line.c_str())) {
            return fail(CURLE_OUT_OF_MEMORY);
        }
    }

    if(!header_lines) {
        return true;
    }

    return easy_setopt(*this, CURLOPT_HTTPHEADER, header_lines.get());
}

bool prepared_request::apply_cookies() noexcept {
    if(spec.cookie.empty()) {
        return true;
    }

    return easy_setopt(*this, CURLOPT_COOKIE, spec.cookie.c_str());
}

bool prepared_request::apply_user_agent() noexcept {
    if(spec.user_agent.empty()) {
        return true;
    }
    return easy_setopt(*this, CURLOPT_USERAGENT, spec.user_agent.c_str());
}

bool prepared_request::apply_redirect() noexcept {
    if(!spec.redirect.follow) {
        return easy_setopt(*this, CURLOPT_FOLLOWLOCATION, 0L);
    }

    return easy_setopt(*this, CURLOPT_FOLLOWLOCATION, 1L) &&
           easy_setopt(*this, CURLOPT_MAXREDIRS, static_cast<long>(spec.redirect.max_redirects)) &&
           easy_setopt(*this, CURLOPT_AUTOREFERER, spec.redirect.referer ? 1L : 0L);
}

bool prepared_request::apply_tls() noexcept {
    if(spec.tls.min_version && spec.tls.max_version &&
       tls_version_rank(*spec.tls.min_version) > tls_version_rank(*spec.tls.max_version)) {
        return fail(error::invalid_request("min tls version must not exceed max tls version"));
    }

#if LIBCURL_VERSION_NUM >= 0x075500
    const char* protocols = spec.tls.https_only ? "https" : "http,https";
    if(!easy_setopt(*this, CURLOPT_PROTOCOLS_STR, protocols) ||
       !easy_setopt(*this, CURLOPT_REDIR_PROTOCOLS_STR, protocols)) {
        return false;
    }
#else
    long protocols =
        spec.tls.https_only ? CURLPROTO_HTTPS : static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS);
    if(!easy_setopt(*this, CURLOPT_PROTOCOLS, protocols) ||
       !easy_setopt(*this, CURLOPT_REDIR_PROTOCOLS, protocols)) {
        return false;
    }
#endif

    if(!easy_setopt(*this,
                    CURLOPT_SSL_VERIFYPEER,
                    spec.tls.danger_accept_invalid_certs ? 0L : 1L) ||
       !easy_setopt(*this,
                    CURLOPT_SSL_VERIFYHOST,
                    spec.tls.danger_accept_invalid_hostnames ? 0L : 2L)) {
        return false;
    }

    if(spec.tls.ca_file && !easy_setopt(*this, CURLOPT_CAINFO, spec.tls.ca_file->c_str())) {
        return false;
    }

    if(spec.tls.ca_path && !easy_setopt(*this, CURLOPT_CAPATH, spec.tls.ca_path->c_str())) {
        return false;
    }

    if(spec.tls.min_version || spec.tls.max_version) {
        long version = CURL_SSLVERSION_DEFAULT;
        if(spec.tls.min_version) {
            version = to_curl_ssl_min(*spec.tls.min_version);
        }
        if(spec.tls.max_version) {
            auto upper = to_curl_ssl_max(*spec.tls.max_version);
            if(upper == 0) {
                return fail(error::invalid_request(
                    "libcurl does not support the requested max tls version"));
            }
            version |= upper;
        }
        if(!easy_setopt(*this, CURLOPT_SSLVERSION, version)) {
            return false;
        }
    }

    return true;
}

bool prepared_request::apply_proxy() noexcept {
    if(spec.disable_proxy) {
        return easy_setopt(*this, CURLOPT_PROXY, "");
    }

    if(!spec.proxy_config) {
        return true;
    }

    const auto& proxy = *spec.proxy_config;
    if(proxy.url.empty()) {
        return fail(error::invalid_request("proxy url must not be empty"));
    }

    if(!easy_setopt(*this, CURLOPT_PROXY, proxy.url.c_str())) {
        return false;
    }

    if(!proxy.username.empty() &&
       !easy_setopt(*this, CURLOPT_PROXYUSERNAME, proxy.username.c_str())) {
        return false;
    }

    if(!proxy.password.empty() &&
       !easy_setopt(*this, CURLOPT_PROXYPASSWORD, proxy.password.c_str())) {
        return false;
    }

    return true;
}

bool prepared_request::apply_timeout() noexcept {
    if(!spec.timeout) {
        return true;
    }

    const auto timeout_ms = spec.timeout->count();
    if(timeout_ms < 0) {
        return fail(error::invalid_request("timeout must be non-negative"));
    }

    if(timeout_ms > (std::numeric_limits<long>::max)()) {
        return fail(error::invalid_request("timeout exceeds libcurl timeout range"));
    }

    return easy_setopt(*this, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
}

bool prepared_request::apply_curl_options() noexcept {
    for(const auto& option: spec.curl_options) {
        if(auto err = option(easy.get()); !curl::ok(err)) {
            return fail(err);
        }
    }

    return true;
}

prepared_request::prepared_request(request req, std::shared_ptr<client_state> owner) noexcept :
    owner(std::move(owner)), spec(std::move(req)) {
    easy = curl::easy_handle::create();
    if(!easy) {
        fail(CURLE_FAILED_INIT);
        return;
    }

    if(this->owner && !this->owner->bind_easy(easy.get(), spec.record_cookie)) {
        fail(error::invalid_request("failed to bind curl easy to client state"));
        return;
    }

    if(!easy_setopt(*this, CURLOPT_WRITEFUNCTION, &prepared_request::on_write) ||
       !easy_setopt(*this, CURLOPT_WRITEDATA, this) ||
       !easy_setopt(*this, CURLOPT_HEADERFUNCTION, &prepared_request::on_header) ||
       !easy_setopt(*this, CURLOPT_HEADERDATA, this)) {
        return;
    }

    if(!apply_url() || !apply_method() || !apply_body() || !apply_headers() || !apply_cookies() ||
       !apply_user_agent() || !apply_redirect() || !apply_tls() || !apply_proxy() ||
       !apply_timeout() || !apply_curl_options()) {
        return;
    }
}

bool prepared_request::ready() const noexcept {
    return easy && result.kind == error_kind::curl && curl::ok(result.curl_code);
}

bool prepared_request::bind_runtime(void* opaque) noexcept {
    if(!ready()) {
        return false;
    }

    if(!easy_setopt(*this, CURLOPT_PRIVATE, opaque)) {
        return false;
    }

    runtime_bound = true;
    return true;
}

outcome<response, error, cancellation> prepared_request::finish() noexcept {
    if(result.kind != error_kind::curl || !curl::ok(result.curl_code)) {
        return outcome<response, error, cancellation>(outcome_error(std::move(result)));
    }

    long status = 0;
    if(auto err = curl::getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &status); !curl::ok(err)) {
        return outcome<response, error, cancellation>(outcome_error(error::from_curl(err)));
    }
    out.status = status;

    char* effective = nullptr;
    if(auto err = curl::getinfo(easy.get(), CURLINFO_EFFECTIVE_URL, &effective);
       curl::ok(err) && effective != nullptr) {
        out.url = effective;
    } else {
        out.url = final_url;
    }

    easy.reset();
    return outcome<response, error, cancellation>(std::move(out));
}

}  // namespace detail

}  // namespace kota::http
