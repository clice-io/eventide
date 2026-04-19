#pragma once

#include "kota/http/curl.h"

namespace kota::http::detail {

curl::easy_error ensure_curl_runtime() noexcept;

}  // namespace kota::http::detail
