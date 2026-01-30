#include "eventide/process.h"

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"

namespace eventide {

namespace {

struct process_wait_tag {};

}  // namespace

process::process(process&& other) noexcept :
    handle(std::move(other)), waiter(other.waiter), active(other.active),
    completed(std::move(other.completed)) {
    other.waiter = nullptr;
    other.active = nullptr;

    if(initialized()) {
        if(auto* handle = as<uv_process_t>()) {
            handle->data = this;
        }
    }
}

process& process::operator=(process&& other) noexcept {
    if(this == &other) {
        return *this;
    }

    handle::operator=(std::move(other));
    waiter = other.waiter;
    active = other.active;
    completed = std::move(other.completed);

    other.waiter = nullptr;
    other.active = nullptr;

    if(initialized()) {
        if(auto* handle = as<uv_process_t>()) {
            handle->data = this;
        }
    }

    return *this;
}

template <>
struct awaiter<process_wait_tag> {
    using promise_t = task<process::wait_result>::promise_type;

    process* self;
    process::exit_status result{};

    static void notify(process& proc, process::exit_status status) {
        proc.completed = status;

        if(proc.waiter && proc.active) {
            *proc.active = status;

            auto w = proc.waiter;
            proc.waiter = nullptr;
            proc.active = nullptr;

            w->resume();
        }
    }

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        self->waiter = waiting ? &waiting.promise() : nullptr;
        self->active = &result;
        return std::noop_coroutine();
    }

    process::wait_result await_resume() noexcept {
        self->waiter = nullptr;
        self->active = nullptr;
        return result;
    }
};

process::stdio process::stdio::inherit() {
    return stdio{};
}

process::stdio process::stdio::ignore() {
    stdio io{};
    io.type = kind::ignore;
    return io;
}

process::stdio process::stdio::from_fd(int fd) {
    stdio io{};
    io.type = kind::fd;
    io.descriptor = fd;
    return io;
}

process::stdio process::stdio::pipe(bool readable, bool writable) {
    stdio io{};
    io.type = kind::pipe;
    io.readable = readable;
    io.writable = writable;
    return io;
}

void on_exit_cb(uv_process_t* handle, int64_t exit_status, int term_signal) {
    auto* proc = static_cast<process*>(handle->data);
    if(proc == nullptr) {
        return;
    }

    awaiter<process_wait_tag>::notify(*proc, process::exit_status{exit_status, term_signal});
}

result<process::spawn_result> process::spawn(event_loop& loop, const options& opts) {
    spawn_result out{process(sizeof(uv_process_t))};

    std::vector<std::string> argv_storage;
    argv_storage.reserve(opts.args.size() + 1);
    argv_storage.push_back(opts.file);
    argv_storage.insert(argv_storage.end(), opts.args.begin(), opts.args.end());

    std::vector<char*> argv;
    argv.reserve(argv_storage.size() + 1);
    for(auto& arg: argv_storage) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    std::vector<std::string> env_storage = opts.env;
    std::vector<char*> envp;
    if(!env_storage.empty()) {
        envp.reserve(env_storage.size() + 1);
        for(auto& e: env_storage) {
            envp.push_back(e.data());
        }
        envp.push_back(nullptr);
    }

    std::array<pipe, 3> created_pipes{};
    std::array<uv_stdio_container_t, 3> stdio{};

    auto make_pipe = [&]() -> result<pipe> {
        pipe out(sizeof(uv_pipe_t));
        int err = uv_pipe_init(static_cast<uv_loop_t*>(loop.handle()), out.as<uv_pipe_t>(), 0);
        if(err != 0) {
            return std::unexpected(error(err));
        }

        out.mark_initialized();
        return out;
    };

    for(std::size_t i = 0; i < opts.streams.size(); ++i) {
        auto& cfg = opts.streams[i];
        auto& dst = stdio[i];

        switch(cfg.type) {
            case stdio::kind::inherit:
                dst.flags = UV_INHERIT_FD;
                dst.data.fd = static_cast<int>(i);
                break;
            case stdio::kind::ignore: dst.flags = UV_IGNORE; break;
            case stdio::kind::fd:
                dst.flags = UV_INHERIT_FD;
                dst.data.fd = cfg.descriptor;
                break;
            case stdio::kind::pipe: {
                auto pipe_res = make_pipe();
                if(!pipe_res.has_value()) {
                    return std::unexpected(pipe_res.error());
                }

                auto* handle = pipe_res->as<uv_pipe_t>();

                dst.flags = UV_CREATE_PIPE;
                if(cfg.readable) {
                    dst.flags =
                        static_cast<uv_stdio_flags>(dst.flags | static_cast<int>(UV_READABLE_PIPE));
                }
                if(cfg.writable) {
                    dst.flags =
                        static_cast<uv_stdio_flags>(dst.flags | static_cast<int>(UV_WRITABLE_PIPE));
                }
                dst.data.stream = reinterpret_cast<uv_stream_t*>(handle);

                created_pipes[i] = std::move(*pipe_res);
                break;
            }
        }
    }

    uv_process_options_t uv_opts{};
    uv_opts.exit_cb = on_exit_cb;
    uv_opts.file = opts.file.c_str();
    uv_opts.args = argv.data();
    uv_opts.stdio_count = static_cast<int>(stdio.size());
    uv_opts.stdio = stdio.data();

    if(!envp.empty()) {
        uv_opts.env = envp.data();
    }

    if(!opts.cwd.empty()) {
        uv_opts.cwd = opts.cwd.c_str();
    }

    uv_opts.flags = 0;
#ifdef UV_PROCESS_DETACHED
    if(opts.detached) {
        uv_opts.flags |= UV_PROCESS_DETACHED;
    }
#endif
#ifdef UV_PROCESS_WINDOWS_HIDE
    if(opts.hide_window) {
        uv_opts.flags |= UV_PROCESS_WINDOWS_HIDE;
    }
#endif

    auto proc_handle = out.proc.as<uv_process_t>();
    int err = uv_spawn(static_cast<uv_loop_t*>(loop.handle()), proc_handle, &uv_opts);
    if(err != 0) {
        out.proc.mark_initialized();
        return std::unexpected(error(err));
    }

    out.proc.mark_initialized();
    proc_handle->data = &out.proc;

    out.stdin_pipe = std::move(created_pipes[0]);
    out.stdout_pipe = std::move(created_pipes[1]);
    out.stderr_pipe = std::move(created_pipes[2]);

    return out;
}

task<process::wait_result> process::wait() {
    if(completed.has_value()) {
        co_return *completed;
    }

    if(waiter != nullptr) {
        co_return std::unexpected(error::socket_is_already_connected);
    }

    co_return co_await awaiter<process_wait_tag>{this};
}

int process::pid() const noexcept {
    if(!initialized()) {
        return -1;
    }

    auto proc = as<const uv_process_t>();
    return proc ? proc->pid : -1;
}

error process::kill(int signum) {
    if(!initialized()) {
        return error::invalid_argument;
    }

    int err = uv_process_kill(as<uv_process_t>(), signum);
    if(err != 0) {
        return error(err);
    }

    return {};
}

}  // namespace eventide
