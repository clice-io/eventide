#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>
#include <vector>

#include "stream_internal.h"

namespace eventide {

namespace {

struct stream_read_await : system_op {
    // Stream state used to register reader waiter and store error status.
    stream::Self* self;

    explicit stream_read_await(stream::Self* state) : self(state) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        detail::cancel_and_complete<stream_read_await>(op, [](auto& aw) {
            if(aw.self) {
                uv::read_stop(aw.self->template as<uv_stream_t>());
                detail::clear_waiter(aw.self, &stream::Self::reader);
            }
        });
    }

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto s = static_cast<eventide::stream::Self*>(handle->data);
        if(!s) {
            buf->base = nullptr;
            buf->len = 0;
            return;
        }

        auto [dst, writable] = s->buffer.get_write_ptr();
        buf->base = dst;
        buf->len = writable;

        if(writable == 0) {
            uv::read_stop(*reinterpret_cast<uv_stream_t*>(handle));
        }
    }

    // When nread=0, it means no data was read but the stream is still alive (e.g., EAGAIN).
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t*) {
        auto s = static_cast<eventide::stream::Self*>(stream->data);
        if(!s || nread < 0) {
            if(s) {
                uv::read_stop(*stream);
                if(s->reader) {
                    auto reader = s->reader;
                    s->reader = nullptr;
                    detail::mark_cancelled_if(reader, nread);
                    s->error_code = detail::status_to_error(nread);
                    reader->complete();
                }
            }
            return;
        }

        s->buffer.advance_write(static_cast<size_t>(nread));

        if(s->reader) {
            auto reader = s->reader;
            s->reader = nullptr;
            s->error_code = {};
            reader->complete();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }

        if(auto err = uv::read_start(self->as<uv_stream_t>(), on_alloc, on_read)) {
            self->error_code = err;
            return waiting;
        }
        self->reader = this;
        return link_continuation(&waiting.promise(), location);
    }

    error await_resume() noexcept {
        return self->error_code;
    }
};

struct stream_read_some_await : system_op {
    using promise_t = task<std::size_t>::promise_type;

    // Stream state that owns the active read waiter.
    stream::Self* self;
    // Destination buffer provided by the caller.
    std::span<char> dst;
    // Number of bytes read by the completion callback.
    std::size_t bytes = 0;

    stream_read_some_await(stream::Self* state, std::span<char> buffer) : self(state), dst(buffer) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        detail::cancel_and_complete<stream_read_some_await>(op, [](auto& aw) {
            if(aw.self) {
                uv::read_stop(aw.self->template as<uv_stream_t>());
                detail::clear_waiter(aw.self, &stream::Self::reader);
            }
        });
    }

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto s = static_cast<eventide::stream::Self*>(handle->data);
        if(!s) {
            buf->base = nullptr;
            buf->len = 0;
            return;
        }

        auto* aw = static_cast<stream_read_some_await*>(s->reader);
        if(!aw || aw->dst.empty()) {
            buf->base = nullptr;
            buf->len = 0;
            return;
        }

        buf->base = aw->dst.data();
        buf->len = static_cast<unsigned int>(aw->dst.size());
    }

    // When nread=0, it means no data was read but the stream is still alive (e.g., EAGAIN).
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t*) {
        auto s = static_cast<eventide::stream::Self*>(stream->data);
        if(!s) {
            return;
        }

        auto* aw = static_cast<stream_read_some_await*>(s->reader);
        if(!aw) {
            return;
        }

        if(nread < 0) {
            aw->bytes = 0;
            detail::mark_cancelled_if(aw, nread);
        } else if(nread > 0) {
            aw->bytes = static_cast<std::size_t>(nread);
        } else {
            // nread=0 with no error means no data was read, but the stream is still alive (e.g.,
            // EAGAIN).
            return;
        }

        uv::read_stop(*stream);
        s->reader = nullptr;
        aw->complete();
    }

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<Promise> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }

        self->reader = this;
        if(auto err = uv::read_start(self->as<uv_stream_t>(), on_alloc, on_read)) {
            self->reader = nullptr;
            return waiting;
        }

        return link_continuation(&waiting.promise(), location);
    }

    std::size_t await_resume() noexcept {
        if(self) {
            self->reader = nullptr;
        }
        return bytes;
    }
};

struct stream_write_await : system_op {
    using promise_t = task<error>::promise_type;

    // Stream state that owns the active write waiter.
    stream::Self* self;
    // Owns outbound bytes until libuv invokes on_write().
    std::vector<char> storage;
    // libuv write request; req.data points back to this awaiter.
    uv_write_t req{};
    // Completion status returned from await_resume().
    error error_code;

    stream_write_await(stream::Self* state, std::span<const char> data) :
        self(state), storage(data.begin(), data.end()) {
        action = &on_cancel;
    }

    static void on_cancel(system_op* op) {
        auto* aw = static_cast<stream_write_await*>(op);
        if(!aw->self) {
            return;
        }
        // uv_write_t is not cancellable via uv_cancel().
        // Keep the request in-flight and wait for on_write() to retire it.
    }

    static void on_write(uv_write_t* req, int status) {
        auto* aw = static_cast<stream_write_await*>(req->data);
        if(!aw || !aw->self) {
            return;
        }

        detail::mark_cancelled_if(aw, status);

        if(status < 0) {
            aw->error_code = detail::status_to_error(status);
        }

        if(aw->self->writer) {
            auto w = aw->self->writer;
            aw->self->writer = nullptr;
            w->complete();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_t> waiting,
                      std::source_location location = std::source_location::current()) noexcept {
        if(!self) {
            return waiting;
        }

        self->writer = this;
        req.data = this;

        uv_buf_t buf = uv::buf_init(storage.empty() ? nullptr : storage.data(),
                                    static_cast<unsigned>(storage.size()));
        if(auto err = uv::write(req,
                                self->as<uv_stream_t>(),
                                std::span<const uv_buf_t>{&buf, 1},
                                on_write)) {
            error_code = err;
            self->writer = nullptr;
            return waiting;
        }

        return link_continuation(&waiting.promise(), location);
    }

    error await_resume() noexcept {
        if(self) {
            self->writer = nullptr;
        }
        return this->error_code;
    }
};

}  // namespace

stream::stream() noexcept : self(nullptr, nullptr) {}

stream::stream(stream&& other) noexcept = default;

stream& stream::operator=(stream&& other) noexcept = default;

stream::~stream() = default;

stream::Self* stream::operator->() noexcept {
    return self.get();
}

void* stream::handle() noexcept {
    return self ? &self->handle : nullptr;
}

const void* stream::handle() const noexcept {
    return self ? &self->handle : nullptr;
}

handle_type guess_handle(int fd) {
    return stream_detail::to_handle_type(uv::guess_handle(fd));
}

task<result<std::string>> stream::read() {
    if(!self) {
        co_return std::unexpected(error::invalid_argument);
    }

    self->bind_stream_userdata();

    if(self->buffer.readable_bytes() == 0) {
        if(auto err = co_await stream_read_await{self.get()}) {
            co_return std::unexpected(err);
        }
    }

    std::string out;
    out.resize(self->buffer.readable_bytes());
    self->buffer.read(out.data(), out.size());
    co_return out;
}

task<std::size_t> stream::read_some(std::span<char> dst) {
    if(!self || dst.empty()) {
        co_return 0;
    }

    if(self->buffer.readable_bytes() != 0) {
        const auto available = self->buffer.readable_bytes();
        const auto to_read = std::min(dst.size(), available);
        self->buffer.read(dst.data(), to_read);
        co_return to_read;
    }

    self->bind_stream_userdata();

    co_return co_await stream_read_some_await{self.get(), dst};
}

task<result<stream::chunk>> stream::read_chunk() {
    chunk out{};
    if(!self) {
        co_return std::unexpected(error::invalid_argument);
    }

    self->bind_stream_userdata();

    if(self->buffer.readable_bytes() == 0) {
        if(auto err = co_await stream_read_await{self.get()}) {
            co_return std::unexpected(err);
        }
    }

    auto [ptr, len] = self->buffer.get_read_ptr();
    out = std::span<const char>(ptr, len);
    co_return out;
}

void stream::consume(std::size_t n) {
    if(!self) {
        return;
    }

    self->buffer.advance_read(n);
}

task<error> stream::write(std::span<const char> data) {
    if(!self || !self->initialized() || data.empty()) {
        co_return error::invalid_argument;
    }

    if(self->writer != nullptr) {
        assert(false && "stream::write supports a single writer at a time");
        co_return error::invalid_argument;
    }

    co_return co_await stream_write_await{self.get(), data};
}

result<std::size_t> stream::try_write(std::span<const char> data) {
    if(!self || !self->initialized()) {
        return std::unexpected(error::invalid_argument);
    }

    if(data.empty()) {
        return std::size_t{0};
    }

    if(data.size() > static_cast<std::size_t>(std::numeric_limits<unsigned>::max())) {
        return std::unexpected(error::value_too_large_for_defined_data_type);
    }

    uv_buf_t buf = uv::buf_init(const_cast<char*>(data.data()), static_cast<unsigned>(data.size()));
    auto res = uv::try_write(self->as<uv_stream_t>(), std::span<const uv_buf_t>{&buf, 1});
    if(!res) {
        return std::unexpected(res.error());
    }

    return *res;
}

bool stream::readable() const noexcept {
    if(!self || !self->initialized()) {
        return false;
    }

    return uv::is_readable(self->as<uv_stream_t>());
}

bool stream::writable() const noexcept {
    if(!self || !self->initialized()) {
        return false;
    }

    return uv::is_writable(self->as<uv_stream_t>());
}

error stream::set_blocking(bool enabled) {
    if(!self || !self->initialized()) {
        return error::invalid_argument;
    }

    if(auto err = uv::stream_set_blocking(self->as<uv_stream_t>(), enabled)) {
        return err;
    }

    return {};
}

stream::stream(Self* state) noexcept : self(state, Self::destroy) {}

}  // namespace eventide
