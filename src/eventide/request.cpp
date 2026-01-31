#include "eventide/request.h"

#include <functional>
#include <memory>

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"
#include "eventide/task.h"

namespace eventide {

namespace {

struct work_op {
    using promise_t = task<error>::promise_type;

    uv_work_t req{};
    work_fn fn;
    error result{};
    promise_t* waiter = nullptr;

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        waiter = waiting ? &waiting.promise() : nullptr;
        return std::noop_coroutine();
    }

    error await_resume() noexcept {
        return result;
    }
};

template <typename Result>
struct fs_op {
    using promise_t = task<result<Result>>::promise_type;

    uv_fs_t req = {};
    std::function<Result(uv_fs_t&)> populate;
    result<Result> out = std::unexpected(error());
    promise_t* waiter = nullptr;

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        waiter = waiting ? &waiting.promise() : nullptr;
        return std::noop_coroutine();
    }

    result<Result> await_resume() noexcept {
        return std::move(out);
    }
};

}  // namespace

task<error> queue(event_loop& loop, work_fn fn) {
    work_op op;
    op.fn = std::move(fn);

    auto work_cb = [](uv_work_t* req) {
        auto* holder = static_cast<work_op*>(req->data);
        if(holder && holder->fn) {
            holder->fn();
        }
    };

    auto after_cb = [](uv_work_t* req, int status) {
        auto* holder = static_cast<work_op*>(req->data);
        if(holder == nullptr) {
            return;
        }

        holder->result = status < 0 ? error(status) : error();
        if(holder->waiter) {
            holder->waiter->resume();
        }
    };

    op.result.clear();
    op.req.data = &op;

    int err = uv_queue_work(static_cast<uv_loop_t*>(loop.handle()), &op.req, work_cb, after_cb);
    if(err != 0) {
        co_return error(err);
    }

    co_return co_await op;
}

fs::dir_handle::dir_handle(dir_handle&& other) noexcept : dir(other.dir) {
    other.dir = nullptr;
}

fs::dir_handle& fs::dir_handle::operator=(dir_handle&& other) noexcept {
    if(this != &other) {
        dir = other.dir;
        other.dir = nullptr;
    }
    return *this;
}

fs::dir_handle::dir_handle(void* ptr) : dir(ptr) {}

bool fs::dir_handle::valid() const noexcept {
    return dir != nullptr;
}

void* fs::dir_handle::native_handle() const noexcept {
    return dir;
}

void fs::dir_handle::reset() noexcept {
    dir = nullptr;
}

fs::dir_handle fs::dir_handle::from_native(void* ptr) {
    return dir_handle(ptr);
}

static fs::dirent::type map_dirent(uv_dirent_type_t t) {
    switch(t) {
        case UV_DIRENT_FILE: return fs::dirent::type::file;
        case UV_DIRENT_DIR: return fs::dirent::type::dir;
        case UV_DIRENT_LINK: return fs::dirent::type::link;
        case UV_DIRENT_FIFO: return fs::dirent::type::fifo;
        case UV_DIRENT_SOCKET: return fs::dirent::type::socket;
        case UV_DIRENT_CHAR: return fs::dirent::type::char_device;
        case UV_DIRENT_BLOCK: return fs::dirent::type::block_device;
        default: return fs::dirent::type::unknown;
    }
}

template <typename Result, typename Submit, typename Populate>
static task<result<Result>> run_fs(event_loop& loop, Submit submit, Populate populate) {
    fs_op<Result> op;
    op.populate = populate;

    auto after_cb = [](uv_fs_t* req) {
        auto* h = static_cast<fs_op<Result>*>(req->data);
        if(h == nullptr) {
            return;
        }

        if(req->result < 0) {
            h->out = std::unexpected(error(static_cast<int>(req->result)));
        } else {
            h->out = h->populate(*req);
        }

        uv_fs_req_cleanup(req);

        if(h->waiter) {
            h->waiter->resume();
        }
    };

    op.req.data = &op;

    int err = submit(op.req, after_cb);
    if(err != 0) {
        co_return std::unexpected(error(err));
    }

    co_return co_await op;
}

static fs::result basic_populate(uv_fs_t& req) {
    fs::result r{};
    r.value = req.result;
    if(req.path) {
        r.path = req.path;
    }
    return r;
}

task<result<fs::result>> fs::unlink(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_unlink(static_cast<uv_loop_t*>(loop.handle()),
                                &req,
                                std::string(path).c_str(),
                                cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::mkdir(event_loop& loop, std::string_view path, int mode) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_mkdir(static_cast<uv_loop_t*>(loop.handle()),
                               &req,
                               std::string(path).c_str(),
                               mode,
                               cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::stat(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_stat(static_cast<uv_loop_t*>(loop.handle()),
                              &req,
                              std::string(path).c_str(),
                              cb);
        },
        basic_populate);
}

task<result<fs::result>>
    fs::copyfile(event_loop& loop, std::string_view path, std::string_view new_path, int flags) {
    auto populate = [&](uv_fs_t& req) {
        fs::result r = basic_populate(req);
        r.path = path;
        r.aux_path = std::string(new_path);
        return r;
    };

    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_copyfile(static_cast<uv_loop_t*>(loop.handle()),
                                  &req,
                                  std::string(path).c_str(),
                                  std::string(new_path).c_str(),
                                  flags,
                                  cb);
        },
        populate);
}

task<result<fs::result>> fs::mkdtemp(event_loop& loop, std::string_view tpl) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_mkdtemp(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 std::string(tpl).c_str(),
                                 cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::mkstemp(event_loop& loop, std::string_view tpl) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_mkstemp(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 std::string(tpl).c_str(),
                                 cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::rmdir(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_rmdir(static_cast<uv_loop_t*>(loop.handle()),
                               &req,
                               std::string(path).c_str(),
                               cb);
        },
        basic_populate);
}

task<result<std::vector<fs::dirent>>> fs::scandir(event_loop& loop,
                                                  std::string_view path,
                                                  int flags) {
    auto populate = [](uv_fs_t& req) {
        std::vector<fs::dirent> out;
        uv_dirent_t ent;
        while(uv_fs_scandir_next(&req, &ent) != error::end_of_file.value()) {
            fs::dirent d;
            if(ent.name) {
                d.name = ent.name;
            }
            d.kind = map_dirent(ent.type);
            out.push_back(std::move(d));
        }
        return out;
    };

    co_return co_await run_fs<std::vector<fs::dirent>>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_scandir(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 std::string(path).c_str(),
                                 flags,
                                 cb);
        },
        populate);
}

task<result<fs::dir_handle>> fs::opendir(event_loop& loop, std::string_view path) {
    auto populate = [](uv_fs_t& req) {
        return fs::dir_handle::from_native(req.ptr);
    };

    co_return co_await run_fs<fs::dir_handle>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_opendir(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 std::string(path).c_str(),
                                 cb);
        },
        populate);
}

task<result<std::vector<fs::dirent>>> fs::readdir(event_loop& loop, fs::dir_handle& dir) {
    if(!dir.valid()) {
        co_return std::unexpected(error::invalid_argument);
    }

    auto dir_ptr = static_cast<uv_dir_t*>(dir.native_handle());
    if(dir_ptr == nullptr) {
        co_return std::unexpected(error::invalid_argument);
    }

    constexpr std::size_t entry_count = 64;
    auto entries_storage = std::make_shared<std::vector<uv_dirent_t>>(entry_count);
    dir_ptr->dirents = entries_storage->data();
    dir_ptr->nentries = entries_storage->size();

    auto populate = [entries_storage](uv_fs_t& req) {
        std::vector<fs::dirent> out;
        auto* d = static_cast<uv_dir_t*>(req.ptr);
        if(d == nullptr) {
            return out;
        }

        for(unsigned i = 0; i < req.result; ++i) {
            auto& ent = d->dirents[i];
            fs::dirent de;
            if(ent.name) {
                de.name = ent.name;
            }
            de.kind = map_dirent(ent.type);
            out.push_back(std::move(de));
        }
        return out;
    };

    co_return co_await run_fs<std::vector<fs::dirent>>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_readdir(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 static_cast<uv_dir_t*>(dir.native_handle()),
                                 cb);
        },
        populate);
}

task<error> fs::closedir(event_loop& loop, fs::dir_handle& dir) {
    if(!dir.valid()) {
        co_return error::invalid_argument;
    }

    auto res = co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_closedir(static_cast<uv_loop_t*>(loop.handle()),
                                  &req,
                                  static_cast<uv_dir_t*>(dir.native_handle()),
                                  cb);
        },
        basic_populate);

    if(!res.has_value()) {
        co_return res.error();
    }

    dir.reset();
    co_return error{};
}

task<result<fs::result>> fs::fstat(event_loop& loop, int fd) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_fstat(static_cast<uv_loop_t*>(loop.handle()), &req, fd, cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::lstat(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_lstat(static_cast<uv_loop_t*>(loop.handle()),
                               &req,
                               std::string(path).c_str(),
                               cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::rename(event_loop& loop,
                                    std::string_view path,
                                    std::string_view new_path) {
    auto populate = [&](uv_fs_t& req) {
        fs::result r = basic_populate(req);
        r.path = path;
        r.aux_path = std::string(new_path);
        return r;
    };

    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_rename(static_cast<uv_loop_t*>(loop.handle()),
                                &req,
                                std::string(path).c_str(),
                                std::string(new_path).c_str(),
                                cb);
        },
        populate);
}

task<result<fs::result>> fs::fsync(event_loop& loop, int fd) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_fsync(static_cast<uv_loop_t*>(loop.handle()), &req, fd, cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::fdatasync(event_loop& loop, int fd) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_fdatasync(static_cast<uv_loop_t*>(loop.handle()), &req, fd, cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::ftruncate(event_loop& loop, int fd, std::int64_t offset) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_ftruncate(static_cast<uv_loop_t*>(loop.handle()), &req, fd, offset, cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::sendfile(event_loop& loop,
                                      int out_fd,
                                      int in_fd,
                                      std::int64_t in_offset,
                                      std::size_t length) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_sendfile(static_cast<uv_loop_t*>(loop.handle()),
                                  &req,
                                  out_fd,
                                  in_fd,
                                  in_offset,
                                  length,
                                  cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::access(event_loop& loop, std::string_view path, int mode) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_access(static_cast<uv_loop_t*>(loop.handle()),
                                &req,
                                std::string(path).c_str(),
                                mode,
                                cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::chmod(event_loop& loop, std::string_view path, int mode) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_chmod(static_cast<uv_loop_t*>(loop.handle()),
                               &req,
                               std::string(path).c_str(),
                               mode,
                               cb);
        },
        basic_populate);
}

task<result<fs::result>>
    fs::utime(event_loop& loop, std::string_view path, double atime, double mtime) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_utime(static_cast<uv_loop_t*>(loop.handle()),
                               &req,
                               std::string(path).c_str(),
                               atime,
                               mtime,
                               cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::futime(event_loop& loop, int fd, double atime, double mtime) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_futime(static_cast<uv_loop_t*>(loop.handle()), &req, fd, atime, mtime, cb);
        },
        basic_populate);
}

task<result<fs::result>>
    fs::lutime(event_loop& loop, std::string_view path, double atime, double mtime) {
    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_lutime(static_cast<uv_loop_t*>(loop.handle()),
                                &req,
                                std::string(path).c_str(),
                                atime,
                                mtime,
                                cb);
        },
        basic_populate);
}

task<result<fs::result>> fs::link(event_loop& loop,
                                  std::string_view path,
                                  std::string_view new_path) {
    auto populate = [&](uv_fs_t& req) {
        fs::result r = basic_populate(req);
        r.path = path;
        r.aux_path = std::string(new_path);
        return r;
    };

    co_return co_await run_fs<fs::result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_link(static_cast<uv_loop_t*>(loop.handle()),
                              &req,
                              std::string(path).c_str(),
                              std::string(new_path).c_str(),
                              cb);
        },
        populate);
}

}  // namespace eventide
