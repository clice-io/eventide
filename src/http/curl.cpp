#include "kota/http/detail/prepared_request.h"
#include "kota/http/detail/runtime.h"
#include "kota/http/detail/util.h"
#include "kota/http/client.h"
#include "kota/http/manager.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "../async/io/awaiter.h"
#include "../async/libuv.h"

namespace kota::http {

namespace {

using manager_table = std::unordered_map<event_loop*, std::unique_ptr<manager>>;

auto& table_mutex() {
    static std::mutex lock;
    return lock;
}

auto& managers() {
    static manager_table table;
    return table;
}

template <typename T>
void require_multi_setopt(CURLM* multi, CURLMoption option, T value, const char* message) noexcept {
    [[maybe_unused]] auto err = curl::multi_setopt(multi, option, value);
    assert(curl::ok(err) && message);
}

constexpr const char* method_name(method verb) noexcept {
    switch(verb) {
        case method::get: return "GET";
        case method::post: return "POST";
        case method::put: return "PUT";
        case method::patch: return "PATCH";
        case method::del: return "DELETE";
        case method::head: return "HEAD";
    }

    return "GET";
}

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

struct manager::timer_context {
    uv_timer_t handle{};
    manager* owner = nullptr;
};

struct manager::socket_context {
    uv_poll_t handle{};
    curl_socket_t socket = 0;
    manager* owner = nullptr;
};

struct request_op : uv::await_op<request_op> {
    using promise_t = task<response, error>::promise_type;
    using result_type = outcome<response, error, cancellation>;

    manager* mgr = nullptr;
    detail::prepared_request* prepared = nullptr;
    bool added = false;
    bool completed = false;

    explicit request_op(manager& manager) : mgr(&manager) {}

    void attach(detail::prepared_request& request) noexcept {
        prepared = &request;
    }

    void mark_removed() noexcept {
        added = false;
    }

    void complete_with(error err, bool resume) noexcept {
        if(completed) {
            return;
        }

        completed = true;
        if(prepared) {
            prepared->result = std::move(err);
        }
        added = false;
        if(resume) {
            this->complete();
        }
    }

    void complete_with(curl::easy_error code, bool resume) noexcept {
        complete_with(error::from_curl(code), resume);
    }

    static void on_cancel(system_op* op) {
        uv::await_op<request_op>::complete_cancel(op, [](request_op& aw) {
            if(aw.added && aw.mgr && aw.prepared && aw.prepared->easy) {
                aw.mgr->remove_request(aw.prepared->easy.get());
                aw.added = false;
            }
            if(aw.prepared) {
                aw.prepared->easy.reset();
            }
            aw.completed = true;
        });
    }

    void start() noexcept {
        if(completed || !prepared) {
            return;
        }

        if(!prepared->prepared || !prepared->easy) {
            completed = true;
            return;
        }

        if(!prepared->runtime_bound) {
            prepared->fail(error::invalid_request("request runtime binding failed"));
            completed = true;
            return;
        }

        if(auto err = mgr->add_request(prepared->easy.get()); !curl::ok(err)) {
            prepared->fail(error::from_curl(curl::to_easy_error(err)));
            completed = true;
            return;
        }

        added = true;
        mgr->drive_timeout_arming(this);
    }

    bool await_ready() const noexcept {
        return completed;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location loc = std::source_location::current()) noexcept {
        return this->link_continuation(&waiting.promise(), loc);
    }

    result_type await_resume() noexcept {
        if(added && mgr && prepared && prepared->easy) {
            mgr->remove_request(prepared->easy.get());
            added = false;
        }

        if(static_cast<async_node&>(*this).state == async_node::Cancelled) {
            return result_type(outcome_cancel(cancellation("http request cancelled")));
        }

        if(!prepared) {
            return result_type(outcome_error(error::invalid_request("missing prepared request")));
        }

        return prepared->finish();
    }

    void finish(curl::easy_error code) noexcept {
        complete_with(code, true);
    }

    void finish_inline(curl::easy_error code) noexcept {
        complete_with(code, false);
    }
};

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

std::size_t prepared_request::on_write(char* data,
                                       std::size_t size,
                                       std::size_t count,
                                       void* userdata) {
    auto* self = static_cast<prepared_request*>(userdata);
    assert(self != nullptr && "curl write callback requires prepared_request");

    const auto bytes = size * count;
    auto* begin = reinterpret_cast<const std::byte*>(data);
    self->out.body.insert(self->out.body.end(), begin, begin + bytes);
    return bytes;
}

std::size_t prepared_request::on_header(char* data,
                                        std::size_t size,
                                        std::size_t count,
                                        void* userdata) {
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
    switch(spec.verb) {
        case method::get:
            return true;
        case method::post:
            return easy_setopt(*this, CURLOPT_POST, 1L);
        case method::put:
        case method::patch:
        case method::del:
            return easy_setopt(*this, CURLOPT_CUSTOMREQUEST, method_name(spec.verb));
        case method::head:
            return easy_setopt(*this, CURLOPT_NOBODY, 1L) &&
                   easy_setopt(*this, CURLOPT_CUSTOMREQUEST, method_name(spec.verb));
    }
    return fail(error::invalid_request("unsupported http method"));
}

bool prepared_request::apply_body() noexcept {
    if(spec.body.empty()) {
        return true;
    }

    return easy_setopt(
               *this, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(spec.body.size())) &&
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

    if(spec.tls.https_only) {
#if LIBCURL_VERSION_NUM >= 0x075500
        if(!easy_setopt(*this, CURLOPT_PROTOCOLS_STR, "https") ||
           !easy_setopt(*this, CURLOPT_REDIR_PROTOCOLS_STR, "https")) {
            return false;
        }
#else
        if(!easy_setopt(*this, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS) ||
           !easy_setopt(*this, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS)) {
            return false;
        }
#endif
    }

    if(!easy_setopt(*this, CURLOPT_SSL_VERIFYPEER, spec.tls.danger_accept_invalid_certs ? 0L : 1L) ||
       !easy_setopt(*this, CURLOPT_SSL_VERIFYHOST,
                    spec.tls.danger_accept_invalid_hostnames ? 0L : 2L)) {
        return false;
    }

    if(spec.tls.ca_file &&
       !easy_setopt(*this, CURLOPT_CAINFO, spec.tls.ca_file->c_str())) {
        return false;
    }

    if(spec.tls.ca_path &&
       !easy_setopt(*this, CURLOPT_CAPATH, spec.tls.ca_path->c_str())) {
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
                return fail(error::invalid_request("libcurl does not support the requested max tls version"));
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

    if(!proxy.username.empty() && !easy_setopt(*this, CURLOPT_PROXYUSERNAME, proxy.username.c_str())) {
        return false;
    }

    if(!proxy.password.empty() && !easy_setopt(*this, CURLOPT_PROXYPASSWORD, proxy.password.c_str())) {
        return false;
    }

    return true;
}

bool prepared_request::apply_timeout() noexcept {
    if(!spec.timeout) {
        return true;
    }

    if(spec.timeout->count() < 0) {
        return fail(error::invalid_request("timeout must be non-negative"));
    }

    return easy_setopt(*this, CURLOPT_TIMEOUT_MS, static_cast<long>(spec.timeout->count()));
}

prepared_request prepared_request::prepare(request req, client_state* owner) noexcept {
    prepared_request prepared;
    prepared.owner = owner;
    prepared.spec = std::move(req);
    prepared.easy = curl::easy_handle::create();
    if(!prepared.easy) {
        prepared.fail(CURLE_FAILED_INIT);
        return prepared;
    }

    if(owner && !owner->bind_easy(prepared.easy.get(), prepared.spec.use_cookie_store)) {
        prepared.fail(error::invalid_request("failed to bind curl easy to client state"));
        return prepared;
    }

    if(!easy_setopt(prepared, CURLOPT_WRITEFUNCTION, &prepared_request::on_write) ||
       !easy_setopt(prepared, CURLOPT_HEADERFUNCTION, &prepared_request::on_header)) {
        return prepared;
    }

    if(!prepared.apply_url() || !prepared.apply_method() || !prepared.apply_body() ||
       !prepared.apply_headers() || !prepared.apply_cookies() || !prepared.apply_user_agent() ||
       !prepared.apply_redirect() || !prepared.apply_tls() || !prepared.apply_proxy() ||
       !prepared.apply_timeout()) {
        return prepared;
    }

    prepared.prepared = true;
    return prepared;
}

bool prepared_request::bind_runtime(void* opaque) noexcept {
    if(!prepared || !easy) {
        return false;
    }

    if(!easy_setopt(*this, CURLOPT_WRITEDATA, this) ||
       !easy_setopt(*this, CURLOPT_HEADERDATA, this) ||
       !easy_setopt(*this, CURLOPT_PRIVATE, opaque)) {
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
    if(auto err = curl::getinfo(easy.get(), CURLINFO_EFFECTIVE_URL, &effective); curl::ok(err) &&
       effective != nullptr) {
        out.url = effective;
    } else {
        out.url = final_url;
    }

    easy.reset();
    return outcome<response, error, cancellation>(std::move(out));
}

}  // namespace detail

manager::manager(event_loop& loop, curl::multi_handle handle) noexcept :
    bound_loop(&loop), multi(std::move(handle)) {
    timer = new timer_context();
    timer->owner = this;
    uv::timer_init(loop, timer->handle);
    timer->handle.data = timer;

    require_multi_setopt(multi.get(),
                         CURLMOPT_SOCKETFUNCTION,
                         &manager::on_curl_socket,
                         "curl socket callback registration failed");
    require_multi_setopt(multi.get(),
                         CURLMOPT_SOCKETDATA,
                         static_cast<void*>(this),
                         "curl socket callback data registration failed");
    require_multi_setopt(multi.get(),
                         CURLMOPT_TIMERFUNCTION,
                         &manager::on_curl_timeout,
                         "curl timer callback registration failed");
    require_multi_setopt(multi.get(),
                         CURLMOPT_TIMERDATA,
                         static_cast<void*>(this),
                         "curl timer callback data registration failed");
}

manager::~manager() {
    close_watchers();
    if(multi) {
        [[maybe_unused]] auto socket_cb =
            curl::multi_setopt(multi.get(), CURLMOPT_SOCKETFUNCTION, nullptr);
        [[maybe_unused]] auto socket_data =
            curl::multi_setopt(multi.get(), CURLMOPT_SOCKETDATA, nullptr);
        [[maybe_unused]] auto timer_cb =
            curl::multi_setopt(multi.get(), CURLMOPT_TIMERFUNCTION, nullptr);
        [[maybe_unused]] auto timer_data =
            curl::multi_setopt(multi.get(), CURLMOPT_TIMERDATA, nullptr);
        multi.reset();
    }
}

manager& manager::for_loop(event_loop& loop) {
    if(auto code = detail::ensure_curl_runtime(); !curl::ok(code)) {
        std::abort();
    }

    std::scoped_lock lock(table_mutex());
    auto& table = managers();

    if(auto it = table.find(&loop); it != table.end()) {
        return *it->second;
    }

    auto multi = curl::multi_handle::create();
    if(!multi) {
        std::abort();
    }

    auto created = std::unique_ptr<manager>(new manager(loop, std::move(multi)));
    auto& out = *created;
    table.emplace(&loop, std::move(created));
    return out;
}

void manager::unregister_loop(event_loop& loop) {
    std::scoped_lock lock(table_mutex());
    managers().erase(&loop);
}

curl::multi_error manager::add_request(CURL* easy) noexcept {
    auto err = curl::multi_add_handle(multi.get(), easy);
    if(curl::ok(err)) {
        active_requests += 1;
    }
    return err;
}

curl::multi_error manager::remove_request(CURL* easy) noexcept {
    auto err = curl::multi_remove_handle(multi.get(), easy);
    if(curl::ok(err) && active_requests > 0) {
        active_requests -= 1;
    }
    return err;
}

void manager::drive_timeout() noexcept {
    int running = 0;
    [[maybe_unused]] auto err =
        curl::multi_socket_action(multi.get(), CURL_SOCKET_TIMEOUT, 0, &running);
    drain_completed();
}

void manager::drive_timeout_arming(void* arming_request) noexcept {
    int running = 0;
    [[maybe_unused]] auto err =
        curl::multi_socket_action(multi.get(), CURL_SOCKET_TIMEOUT, 0, &running);
    drain_completed_arming(arming_request);
}

void manager::drive_socket(curl_socket_t socket, int flags) noexcept {
    int running = 0;
    [[maybe_unused]] auto err = curl::multi_socket_action(multi.get(), socket, flags, &running);
    drain_completed();
}

void manager::drain_completed() noexcept {
    drain_completed_impl(nullptr);
}

void manager::drain_completed_arming(void* arming_request) noexcept {
    drain_completed_impl(arming_request);
}

void manager::drain_completed_impl(void* arming_request) noexcept {
    int pending = 0;
    while(auto* msg = curl::multi_info_read(multi.get(), &pending)) {
        if(msg->msg != CURLMSG_DONE) {
            continue;
        }

        auto* easy = msg->easy_handle;
        void* opaque = nullptr;
        if(auto err = curl::getinfo(easy, CURLINFO_PRIVATE, &opaque); !curl::ok(err) || !opaque) {
            continue;
        }

        auto* req = static_cast<request_op*>(opaque);
        remove_request(easy);
        req->mark_removed();
        if(arming_request != nullptr && opaque == arming_request) {
            req->finish_inline(msg->data.result);
            continue;
        }

        req->finish(msg->data.result);
    }
}

manager::socket_context* manager::ensure_socket(curl_socket_t socket) noexcept {
    if(auto it = sockets.find(socket); it != sockets.end()) {
        return it->second;
    }

    auto* context = new socket_context();
    context->socket = socket;
    context->owner = this;

    if(auto err = uv::poll_init_socket(loop(), context->handle, static_cast<uv_os_sock_t>(socket));
       err) {
        delete context;
        return nullptr;
    }

    context->handle.data = context;
    sockets.emplace(socket, context);
    return context;
}

void manager::update_socket(curl_socket_t socket, int action, void* socketp) noexcept {
    switch(action) {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT: {
            auto* context = socketp ? static_cast<socket_context*>(socketp) : ensure_socket(socket);
            if(!context) {
                return;
            }

            [[maybe_unused]] auto assign =
                curl::multi_assign(multi.get(), socket, static_cast<void*>(context));

            int events = 0;
            if(action != CURL_POLL_IN) {
                events |= UV_WRITABLE;
            }
            if(action != CURL_POLL_OUT) {
                events |= UV_READABLE;
            }

            [[maybe_unused]] auto err =
                uv::poll_start(context->handle, events, &manager::on_uv_socket);
            return;
        }

        case CURL_POLL_REMOVE: {
            auto* context = socketp ? static_cast<socket_context*>(socketp) : nullptr;
            if(!context) {
                if(auto it = sockets.find(socket); it != sockets.end()) {
                    context = it->second;
                }
            }
            if(!context) {
                return;
            }

            sockets.erase(context->socket);
            context->owner = nullptr;
            [[maybe_unused]] auto err = uv::poll_stop(context->handle);
            [[maybe_unused]] auto assign = curl::multi_assign(multi.get(), socket, nullptr);
            if(!uv::is_closing(context->handle)) {
                uv::close(context->handle, &manager::on_uv_socket_close);
            }
            return;
        }

        default: return;
    }
}

void manager::update_timeout(long timeout_ms) noexcept {
    if(!timer) {
        return;
    }

    if(timeout_ms < 0) {
        uv::timer_stop(timer->handle);
        return;
    }

    if(timeout_ms == 0) {
        timeout_ms = 1;
    }

    uv::timer_start(timer->handle,
                    &manager::on_uv_timeout,
                    static_cast<std::uint64_t>(timeout_ms),
                    0);
}

void manager::close_watchers() noexcept {
    for(auto& [socket, context]: sockets) {
        (void)socket;
        if(!context) {
            continue;
        }
        context->owner = nullptr;
        [[maybe_unused]] auto stop = uv::poll_stop(context->handle);
        [[maybe_unused]] auto assign = curl::multi_assign(multi.get(), context->socket, nullptr);
        if(!uv::is_closing(context->handle)) {
            uv::close(context->handle, &manager::on_uv_socket_close);
        }
    }
    sockets.clear();

    if(timer) {
        timer->owner = nullptr;
        uv::timer_stop(timer->handle);
        if(!uv::is_closing(timer->handle)) {
            uv::close(timer->handle, &manager::on_uv_timer_close);
        }
        timer = nullptr;
    }
}

int manager::on_curl_socket(CURL*,
                            curl_socket_t socket,
                            int action,
                            void* userp,
                            void* socketp) noexcept {
    auto* self = static_cast<manager*>(userp);
    if(!self) {
        return 0;
    }

    self->update_socket(socket, action, socketp);
    return 0;
}

int manager::on_curl_timeout(CURLM*, long timeout_ms, void* userp) noexcept {
    auto* self = static_cast<manager*>(userp);
    if(!self) {
        return 0;
    }

    self->update_timeout(timeout_ms);
    return 0;
}

void manager::on_uv_socket(uv_poll_t* handle, int status, int events) noexcept {
    auto* context = static_cast<socket_context*>(handle->data);
    if(!context || !context->owner) {
        return;
    }

    int flags = 0;
    if(status < 0) {
        flags |= CURL_CSELECT_ERR;
    }
    if(events & UV_READABLE) {
        flags |= CURL_CSELECT_IN;
    }
    if(events & UV_WRITABLE) {
        flags |= CURL_CSELECT_OUT;
    }

    context->owner->drive_socket(context->socket, flags);
}

void manager::on_uv_timeout(uv_timer_t* handle) noexcept {
    auto* context = static_cast<timer_context*>(handle->data);
    if(!context || !context->owner) {
        return;
    }

    context->owner->drive_timeout();
}

void manager::on_uv_socket_close(uv_handle_t* handle) noexcept {
    auto* context = static_cast<socket_context*>(handle->data);
    delete context;
}

void manager::on_uv_timer_close(uv_handle_t* handle) noexcept {
    auto* context = static_cast<timer_context*>(handle->data);
    delete context;
}

task<response, error> detail::execute_with_state(request req,
                                                 event_loop& loop,
                                                 client_state* owner) {
    auto prepared = detail::prepared_request::prepare(std::move(req), owner);
    request_op op(manager::for_loop(loop));
    op.attach(prepared);
    if(!prepared.bind_runtime(&op) && prepared.result.kind == error_kind::curl &&
       curl::ok(prepared.result.curl_code)) {
        prepared.result = error::invalid_request("failed to bind request to runtime");
    }

    op.start();
    auto result = co_await op;

    if(result.is_cancelled()) {
        co_await cancel();
    }

    if(result.has_error()) {
        co_await fail(std::move(result).error());
    }

    co_return std::move(*result);
}

}  // namespace kota::http
