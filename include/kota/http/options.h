#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "kota/http/common.h"

namespace kota::http {

struct client_options {
    std::vector<header> default_headers;
    std::string default_cookie;
    std::optional<proxy> proxy_config;
    std::string user_agent;
    redirect_policy redirect = redirect_policy::limited();
    tls_options tls{};
    std::optional<std::chrono::milliseconds> timeout;
    bool use_cookie_store = true;
    bool disable_proxy = false;
};

}  // namespace kota::http
