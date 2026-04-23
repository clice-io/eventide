#include <memory>
#include <source_location>

#include "../async/io/awaiter.h"
#include "kota/http/detail/client.h"
#include "kota/http/detail/manager.h"
#include "kota/http/detail/prepared_request.h"
#include "kota/http/detail/runtime.h"

namespace kota::http {

namespace detail {

void clear_runtime_binding(prepared_request& prepared) noexcept {
    if(!prepared.easy) {
        prepared.runtime_bound = false;
        return;
    }

    [[maybe_unused]] auto err = curl::setopt(prepared.easy.get(), CURLOPT_PRIVATE, nullptr);
    prepared.runtime_bound = false;
}

struct request_op;

struct request_runtime_state : std::enable_shared_from_this<request_runtime_state> {
    manager* mgr = nullptr;
    prepared_request* prepared = nullptr;
    request_op* op = nullptr;
    bool added = false;
    bool completed = false;

    void mark_removed() noexcept {
        added = false;
    }

    void complete_with(error err, bool resume) noexcept;

    void complete_with(curl::easy_error code, bool resume) noexcept {
        complete_with(error::from_curl(code), resume);
    }
};

struct request_op : uv::await_op<request_op> {
    using promise_t = task<response, error>::promise_type;
    using result_type = outcome<response, error, cancellation>;

    request_runtime_ref runtime;

    explicit request_op(manager& manager) : runtime(make_request_runtime_state()) {
        runtime->mgr = &manager;
        runtime->op = this;
    }

    ~request_op() {
        if(!runtime) {
            return;
        }

        if(runtime->prepared) {
            clear_runtime_binding(*runtime->prepared);
        }

        if(runtime->added && runtime->mgr && runtime->prepared && runtime->prepared->easy) {
            runtime->mgr->remove_request(runtime->prepared->easy.get());
            runtime->added = false;
        }

        runtime->prepared = nullptr;
        runtime->op = nullptr;
        runtime->mgr = nullptr;
    }

    void attach(prepared_request& request) noexcept {
        runtime->prepared = &request;
    }

    static void on_cancel(system_op* op) {
        uv::await_op<request_op>::complete_cancel(op, [](request_op& aw) {
            if(aw.runtime->prepared) {
                clear_runtime_binding(*aw.runtime->prepared);
            }
            if(aw.runtime->added && aw.runtime->mgr && aw.runtime->prepared &&
               aw.runtime->prepared->easy) {
                aw.runtime->mgr->remove_request(aw.runtime->prepared->easy.get());
                aw.runtime->added = false;
            }
            if(aw.runtime->prepared) {
                aw.runtime->prepared->easy.reset();
            }
            aw.runtime->completed = true;
        });
    }

    void start() noexcept {
        if(runtime->completed || !runtime->prepared) {
            return;
        }

        if(!runtime->prepared->ready()) {
            runtime->completed = true;
            return;
        }

        if(!runtime->prepared->runtime_bound) {
            runtime->prepared->fail(error::invalid_request("request runtime binding failed"));
            runtime->completed = true;
            return;
        }

        if(auto err = runtime->mgr->add_request(runtime->prepared->easy.get()); !curl::ok(err)) {
            runtime->prepared->fail(error::from_curl(curl::to_easy_error(err)));
            runtime->completed = true;
            return;
        }

        runtime->added = true;
        runtime->mgr->drive_timeout_arming(request_runtime_opaque(runtime));
    }

    bool await_ready() const noexcept {
        return runtime->completed;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location loc = std::source_location::current()) noexcept {
        return this->link_continuation(&waiting.promise(), loc);
    }

    result_type await_resume() noexcept {
        if(runtime->added && runtime->mgr && runtime->prepared && runtime->prepared->easy) {
            runtime->mgr->remove_request(runtime->prepared->easy.get());
            runtime->added = false;
        }

        if(static_cast<async_node&>(*this).state == async_node::Cancelled) {
            return result_type(outcome_cancel(cancellation("http request cancelled")));
        }

        if(!runtime->prepared) {
            return result_type(outcome_error(error::invalid_request("missing prepared request")));
        }

        return runtime->prepared->finish();
    }

    void finish(curl::easy_error code) noexcept {
        runtime->complete_with(code, true);
    }

    void finish_inline(curl::easy_error code) noexcept {
        runtime->complete_with(code, false);
    }
};

void request_runtime_state::complete_with(error err, bool resume) noexcept {
    if(completed) {
        return;
    }

    completed = true;
    if(prepared) {
        prepared->result = std::move(err);
    }
    added = false;

    auto* waiting = op;
    if(resume && waiting) {
        waiting->complete();
    }
}

request_runtime_ref make_request_runtime_state() noexcept {
    return std::make_shared<request_runtime_state>();
}

void* request_runtime_opaque(const request_runtime_ref& runtime) noexcept {
    return runtime.get();
}

request_runtime_ref retain_request_operation(void* opaque) noexcept {
    auto* runtime = static_cast<request_runtime_state*>(opaque);
    if(!runtime) {
        return {};
    }

    return runtime->weak_from_this().lock();
}

void mark_request_operation_removed(const request_runtime_ref& runtime) noexcept {
    if(!runtime) {
        return;
    }

    runtime->mark_removed();
}

void complete_request_operation(const request_runtime_ref& runtime,
                                curl::easy_error result,
                                bool resume_inline) noexcept {
    if(!runtime) {
        return;
    }

    if(resume_inline) {
        runtime->complete_with(result, false);
        return;
    }

    runtime->complete_with(result, true);
}

task<response, error> execute_with_state(request req,
                                         event_loop& loop,
                                         std::shared_ptr<client_state> owner) {
    auto manager = manager::try_for_loop(loop);
    if(!manager) {
        co_await fail(std::move(manager.error()));
    }

    prepared_request prepared(std::move(req), owner);
    request_op op(manager->get());
    op.attach(prepared);
    if(!prepared.bind_runtime(request_runtime_opaque(op.runtime)) &&
       prepared.result.kind == error_kind::curl && curl::ok(prepared.result.curl_code)) {
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

}  // namespace detail

}  // namespace kota::http
