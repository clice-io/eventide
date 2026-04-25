#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kota/http/detail/common.h"

namespace kota::http {

struct response {
    int status = 0;
    std::string url;
    std::vector<header> headers;
    std::vector<std::byte> body;

    bool ok() const noexcept {
        return 200 <= status && status < 300;
    }

    std::span<const std::byte> bytes() const noexcept;

    std::string_view text() const noexcept;

    std::string text_copy() const;

    std::optional<std::string_view> header_value(std::string_view name) const noexcept;
};

}  // namespace kota::http
