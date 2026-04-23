#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <optional>

#include "kota/http/detail/common.h"
#include "kota/http/detail/curl.h"
#include "kota/http/detail/options.h"
#include "kota/async/io/loop.h"

namespace kota::http {

struct client_state {
    static std::expected<std::shared_ptr<client_state>, error> create(client_options opts);

    void bind(event_loop& loop) noexcept;

    bool is_bound() const noexcept;

    std::optional<std::reference_wrapper<event_loop>> loop() const noexcept;

    const client_options& options() const noexcept;

    void record_cookie(bool enabled) noexcept;

    bool bind_easy(CURL* easy, bool enable_record_cookie) const noexcept;

    // A client is permanently attached to at most one event_loop, so libcurl's
    // shared cookie/DNS/SSL state never crosses threads inside the HTTP module.
    event_loop* bound_loop = nullptr;
    client_options defaults{};
    curl::share_handle share{};

private:
    explicit client_state(client_options opts);
};

}  // namespace kota::http
