#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "kota/http/common.h"

namespace kota::http {

struct request {
    method verb = method::get;
    std::string url;
    std::vector<header> headers;
    std::string cookie;
    std::vector<query_param> query;
    std::string body;
    std::string user_agent;
    std::optional<proxy> proxy_config;
    redirect_policy redirect = redirect_policy::limited();
    tls_options tls{};
    std::optional<std::chrono::milliseconds> timeout;
    bool use_cookie_store = true;
    bool disable_proxy = false;
};

}  // namespace kota::http
