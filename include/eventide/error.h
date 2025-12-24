#pragma once

#include <system_error>

namespace eventide {

std::error_code uv_error(int errc);

}  // namespace eventide
