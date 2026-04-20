#pragma once

#include <string>

#include "kota/http/detail/client_state.h"
#include "kota/http/request.h"
#include "kota/http/response.h"
#include "kota/async/runtime/task.h"
#include "kota/async/vocab/outcome.h"

namespace kota::http::detail {

struct prepared_request {
    client_state* owner = nullptr;
    request spec{};
    curl::easy_handle easy;
    curl::slist header_lines;
    response out{};
    error result{};
    std::string final_url;
    bool prepared = false;
    bool runtime_bound = false;

    static std::size_t on_write(char* data, std::size_t size, std::size_t count, void* userdata);

    static std::size_t on_header(char* data, std::size_t size, std::size_t count, void* userdata);

    bool fail(error err) noexcept;

    bool fail(curl::easy_error code) noexcept;

    bool apply_url() noexcept;

    bool apply_method() noexcept;

    bool apply_body() noexcept;

    bool apply_headers() noexcept;

    bool apply_cookies() noexcept;

    bool apply_user_agent() noexcept;

    bool apply_redirect() noexcept;

    bool apply_tls() noexcept;

    bool apply_proxy() noexcept;

    bool apply_timeout() noexcept;

    static prepared_request prepare(request req, client_state* owner) noexcept;

    bool bind_runtime(void* opaque) noexcept;

    outcome<response, error, cancellation> finish() noexcept;
};

template <typename T>
bool easy_setopt(prepared_request& prepared, CURLoption option, T value) noexcept {
    if(auto err = curl::setopt(prepared.easy.get(), option, value); !curl::ok(err)) {
        return prepared.fail(err);
    }
    return true;
}

}  // namespace kota::http::detail
