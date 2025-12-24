#include "eventide/stream.h"

#include "libuv.h"
#include "eventide/loop.h"

namespace eventide {

namespace awaiter {

struct read {
    stream& target;

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto s = static_cast<eventide::stream*>(handle->data);
        if(!s) {
            buf->base = nullptr;
            buf->len = 0;
            return;
        }

        auto [dst, writable] = s->buffer.get_write_ptr();
        buf->base = dst;
        buf->len = writable;

        if(writable == 0) {
            uv_read_stop(reinterpret_cast<uv_stream_t*>(handle));
        }
    }

    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
        auto s = static_cast<eventide::stream*>(stream->data);
        if(!s || nread <= 0) {
            if(s) {
                uv_read_stop(stream);
                if(s->reader) {
                    s->reader->resume();
                }
            }
            return;
        }

        s->buffer.advance_write(static_cast<size_t>(nread));

        if(s->reader) {
            s->reader->resume();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> waiting) noexcept {
        target.reader = &waiting.promise();
        int err = uv_read_start(target.as<uv_stream_t>(), on_alloc, on_read);
        return std::noop_coroutine();
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

task<std::string> stream::read() {
    auto stream = as<uv_stream_t>();
    stream->data = this;

    if(buffer.readable_bytes() == 0) {
        co_await awaiter::read(*this);
    }

    std::string out;
    out.resize(buffer.readable_bytes());
    buffer.read(out.data(), out.size());
    co_return out;
}

task<> stream::write(std::span<const char> data) {
    return task<>();
}

std::expected<pipe, std::error_code> pipe::open(event_loop& loop, int fd) {
    auto h = pipe(sizeof(uv_pipe_t));

    auto handle = h.as<uv_pipe_t>();
    int errc = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), handle, 0);
    if(errc != 0) {
        return std::unexpected(uv_error(errc));
    }

    h.mark_initialized();

    errc = uv_pipe_open(handle, fd);
    if(errc != 0) {
        return std::unexpected(uv_error(errc));
    }

    return h;
}

}  // namespace eventide
