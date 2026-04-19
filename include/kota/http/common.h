#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "kota/http/curl.h"

namespace kota::http {

enum class method {
    get,
    post,
    put,
    patch,
    del,
    head,
};

enum class error_kind {
    curl,
    invalid_request,
    json_encode,
};

struct error {
    error_kind kind = error_kind::curl;
    curl::easy_error curl_code = CURLE_OK;
    std::string detail;

    static error from_curl(curl::easy_error code) {
        return {.kind = error_kind::curl, .curl_code = code, .detail = {}};
    }

    static error invalid_request(std::string detail) {
        return {.kind = error_kind::invalid_request, .detail = std::move(detail)};
    }

    static error json_encode(std::string detail) {
        return {.kind = error_kind::json_encode, .detail = std::move(detail)};
    }
};

std::string message(const error& err);

struct header {
    std::string name;
    std::string value;
};

struct query_param {
    std::string name;
    std::string value;
};

struct proxy {
    std::string url;
    std::string username;
    std::string password;
};

struct redirect_policy {
    bool follow = true;
    std::size_t max_redirects = 10;
    bool referer = true;

    static redirect_policy none() noexcept {
        return {.follow = false, .max_redirects = 0, .referer = false};
    }

    static redirect_policy limited(std::size_t max_redirects = 10) noexcept {
        return {.follow = true, .max_redirects = max_redirects, .referer = true};
    }
};

enum class tls_version {
    tls1_0,
    tls1_1,
    tls1_2,
    tls1_3,
};

struct tls_options {
    bool https_only = false;
    bool danger_accept_invalid_certs = false;
    bool danger_accept_invalid_hostnames = false;
    std::optional<std::string> ca_file;
    std::optional<std::string> ca_path;
    std::optional<tls_version> min_version;
    std::optional<tls_version> max_version;
};

}  // namespace kota::http
