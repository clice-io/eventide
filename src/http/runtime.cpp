#include "kota/http/detail/runtime.h"

namespace kota::http::detail {

curl::easy_error ensure_curl_runtime() noexcept {
    struct runtime_state {
        runtime_state() noexcept : code(curl::global_init()) {}

        ~runtime_state() {
            if(curl::ok(code)) {
                curl::global_cleanup();
            }
        }

        curl::easy_error code = CURLE_OK;
    };

    static runtime_state runtime;
    return runtime.code;
}

}  // namespace kota::http::detail
