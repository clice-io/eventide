#include "eventide/stream.h"

#include "libuv.h"
#include "eventide/loop.h"

namespace eventide {

namespace awaiter {

template <typename Derived>
struct read {
    stream<Derived>& target;

    static void on_alloc(uv_handle_t* handle, size_t, uv_buf_t* buf) {
        auto s = static_cast<eventide::stream<Derived>*>(handle->data);
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
        auto s = static_cast<eventide::stream<Derived>*>(stream->data);
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
        int err = uv_read_start((uv_stream_t*)target.storage, on_alloc, on_read);
        return std::noop_coroutine();
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

template <typename Derived>
task<std::string> stream<Derived>::read() {
    auto stream = (uv_stream_t*)this->storage;
    stream->data = this;

    if(buffer.readable_bytes() == 0) {
        co_await awaiter::read<Derived>(*this);
    }

    std::string out;
    out.resize(buffer.readable_bytes());
    buffer.read(out.data(), out.size());
    co_return out;
}

template <typename Derived>
task<> stream<Derived>::write(std::span<const char> data) {
    return task<>();
}

template class stream<pipe>;
template class stream<tcp_socket>;
template class stream<console>;

pipe::pipe(event_loop& loop, int fd) {
    auto p = reinterpret_cast<uv_pipe_t*>(storage);
    uv_pipe_init((uv_loop_t*)loop.native_handle(), p, 0);
    uv_pipe_open(p, fd);
}

}  // namespace eventide
