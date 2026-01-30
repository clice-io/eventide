#include "eventide/request.h"

#include <functional>
#include <memory>
#include <optional>

#include "libuv.h"
#include "eventide/error.h"
#include "eventide/loop.h"
#include "eventide/task.h"

namespace {

struct work_tag {};

struct fs_tag {};

template <typename Result>
struct fs_holder {
    uv_fs_t req;
    std::function<Result(uv_fs_t&)> populate;
    ::eventide::result<Result> out = std::unexpected(::eventide::error{});
};

}  // namespace

namespace eventide {

template <>
struct awaiter<work_tag> {
    using promise_t = task<error>::promise_type;

    promise_t* waiter = nullptr;
    error result{};

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

task<error> work_request::queue(event_loop& loop, work_fn fn) {
    awaiter<work_tag> aw;

    struct work_holder {
        uv_work_t req{};
        work_fn fn;
        awaiter<work_tag>* aw = nullptr;
    };

    auto work_cb = [](uv_work_t* req) {
        auto* holder = static_cast<work_holder*>(req->data);
        if(holder && holder->fn) {
            holder->fn();
        }
    };

    auto after_cb = [](uv_work_t* req, int status) {
        auto* holder = static_cast<work_holder*>(req->data);
        if(holder == nullptr) {
            return;
        }

        if(holder->aw) {
            holder->aw->result = status < 0 ? error(status) : error();
            if(holder->aw->waiter) {
                holder->aw->waiter->resume();
            }
        }

        delete holder;
    };

    aw.result.clear();

    auto holder = std::make_unique<work_holder>();
    holder->fn = std::move(fn);
    holder->aw = &aw;
    holder->req.data = holder.get();
    auto* raw = holder.release();

    int err = uv_queue_work(static_cast<uv_loop_t*>(loop.handle()), &raw->req, work_cb, after_cb);
    if(err != 0) {
        delete raw;
        co_return error(err);
    }

    co_await aw;
    co_return aw.result;
}

template <typename Result>
struct fs_awaiter {
    using promise_t = task<result<Result>>::promise_type;

    promise_t* waiter = nullptr;
    result<Result>* target = nullptr;

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_t> waiting) noexcept {
        waiter = waiting ? &waiting.promise() : nullptr;
        return std::noop_coroutine();
    }

    result<Result> await_resume() noexcept {
        return std::move(*target);
    }
};

fs_request::dir_handle::dir_handle(dir_handle&& other) noexcept : dir(other.dir) {
    other.dir = nullptr;
}

fs_request::dir_handle& fs_request::dir_handle::operator=(dir_handle&& other) noexcept {
    if(this != &other) {
        dir = other.dir;
        other.dir = nullptr;
    }
    return *this;
}

fs_request::dir_handle::dir_handle(void* ptr) : dir(ptr) {}

bool fs_request::dir_handle::valid() const noexcept {
    return dir != nullptr;
}

static fs_request::dirent::type map_dirent(uv_dirent_type_t t) {
    switch(t) {
        case UV_DIRENT_FILE: return fs_request::dirent::type::file;
        case UV_DIRENT_DIR: return fs_request::dirent::type::dir;
        case UV_DIRENT_LINK: return fs_request::dirent::type::link;
        case UV_DIRENT_FIFO: return fs_request::dirent::type::fifo;
        case UV_DIRENT_SOCKET: return fs_request::dirent::type::socket;
        case UV_DIRENT_CHAR: return fs_request::dirent::type::char_device;
        case UV_DIRENT_BLOCK: return fs_request::dirent::type::block_device;
        default: return fs_request::dirent::type::unknown;
    }
}

template <typename Result, typename Submit, typename Populate>
static task<result<Result>> run_fs(event_loop& loop, Submit submit, Populate populate) {
    auto holder = std::make_unique<fs_holder<Result>>();
    fs_awaiter<Result> aw;
    aw.target = &holder->out;

    holder->populate = populate;

    auto after_cb = [](uv_fs_t* req) {
        auto* h = static_cast<fs_holder<Result>*>(req->data);
        auto* aw_ptr = static_cast<fs_awaiter<Result>*>(req->reserved[0]);

        if(h == nullptr || aw_ptr == nullptr) {
            return;
        }

        if(req->result < 0) {
            h->out = std::unexpected(error(static_cast<int>(req->result)));
        } else {
            h->out = h->populate(*req);
        }

        uv_fs_req_cleanup(req);

        if(aw_ptr->waiter) {
            aw_ptr->waiter->resume();
        }
    };

    holder->req.data = holder.get();
    holder->req.reserved[0] = &aw;

    int err = submit(holder->req, after_cb);
    if(err != 0) {
        co_return std::unexpected(error(err));
    }

    co_return co_await aw;
}

static fs_request::result basic_populate(uv_fs_t& req) {
    fs_request::result r{};
    r.value = req.result;
    if(req.path) {
        r.path = req.path;
    }
    return r;
}

task<result<fs_request::result>> fs_request::unlink(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_unlink(static_cast<uv_loop_t*>(loop.handle()),
                                &req,
                                std::string(path).c_str(),
                                cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::mkdir(event_loop& loop,
                                                   std::string_view path,
                                                   int mode) {
    co_return co_await run_fs<result>(
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

task<result<fs_request::result>> fs_request::stat(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_stat(static_cast<uv_loop_t*>(loop.handle()),
                              &req,
                              std::string(path).c_str(),
                              cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::copyfile(event_loop& loop,
                                                      std::string_view path,
                                                      std::string_view new_path,
                                                      int flags) {
    auto populate = [&](uv_fs_t& req) {
        result r = basic_populate(req);
        r.path = path;
        r.aux_path = std::string(new_path);
        return r;
    };

    co_return co_await run_fs<result>(
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

task<result<fs_request::result>> fs_request::mkdtemp(event_loop& loop, std::string_view tpl) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_mkdtemp(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 std::string(tpl).c_str(),
                                 cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::mkstemp(event_loop& loop, std::string_view tpl) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_mkstemp(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 std::string(tpl).c_str(),
                                 cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::rmdir(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_rmdir(static_cast<uv_loop_t*>(loop.handle()),
                               &req,
                               std::string(path).c_str(),
                               cb);
        },
        basic_populate);
}

task<result<std::vector<fs_request::dirent>>> fs_request::scandir(event_loop& loop,
                                                                  std::string_view path,
                                                                  int flags) {
    auto populate = [](uv_fs_t& req) {
        std::vector<dirent> out;
        uv_dirent_t ent;
        while(uv_fs_scandir_next(&req, &ent) != error::end_of_file.value()) {
            dirent d;
            if(ent.name) {
                d.name = ent.name;
            }
            d.kind = map_dirent(ent.type);
            out.push_back(std::move(d));
        }
        return out;
    };

    co_return co_await run_fs<std::vector<dirent>>(
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

task<result<fs_request::dir_handle>> fs_request::opendir(event_loop& loop, std::string_view path) {
    auto populate = [](uv_fs_t& req) {
        return dir_handle{req.ptr};
    };

    co_return co_await run_fs<dir_handle>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_opendir(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 std::string(path).c_str(),
                                 cb);
        },
        populate);
}

task<result<std::vector<fs_request::dirent>>> fs_request::readdir(event_loop& loop,
                                                                  dir_handle& dir) {
    if(!dir.valid()) {
        co_return std::unexpected(error::invalid_argument);
    }

    auto dir_ptr = static_cast<uv_dir_t*>(dir.dir);
    if(dir_ptr == nullptr) {
        co_return std::unexpected(error::invalid_argument);
    }

    constexpr std::size_t entry_count = 64;
    auto entries_storage = std::make_shared<std::vector<uv_dirent_t>>(entry_count);
    dir_ptr->dirents = entries_storage->data();
    dir_ptr->nentries = entries_storage->size();

    auto populate = [entries_storage](uv_fs_t& req) {
        std::vector<dirent> out;
        auto* d = static_cast<uv_dir_t*>(req.ptr);
        if(d == nullptr) {
            return out;
        }

        for(unsigned i = 0; i < req.result; ++i) {
            auto& ent = d->dirents[i];
            dirent de;
            if(ent.name) {
                de.name = ent.name;
            }
            de.kind = map_dirent(ent.type);
            out.push_back(std::move(de));
        }
        return out;
    };

    co_return co_await run_fs<std::vector<dirent>>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_readdir(static_cast<uv_loop_t*>(loop.handle()),
                                 &req,
                                 static_cast<uv_dir_t*>(dir.dir),
                                 cb);
        },
        populate);
}

task<error> fs_request::closedir(event_loop& loop, dir_handle& dir) {
    if(!dir.valid()) {
        co_return error::invalid_argument;
    }

    auto res = co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_closedir(static_cast<uv_loop_t*>(loop.handle()),
                                  &req,
                                  static_cast<uv_dir_t*>(dir.dir),
                                  cb);
        },
        basic_populate);

    if(!res.has_value()) {
        co_return res.error();
    }

    dir.dir = nullptr;
    co_return error{};
}

task<result<fs_request::result>> fs_request::fstat(event_loop& loop, int fd) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_fstat(static_cast<uv_loop_t*>(loop.handle()), &req, fd, cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::lstat(event_loop& loop, std::string_view path) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_lstat(static_cast<uv_loop_t*>(loop.handle()),
                               &req,
                               std::string(path).c_str(),
                               cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::rename(event_loop& loop,
                                                    std::string_view path,
                                                    std::string_view new_path) {
    auto populate = [&](uv_fs_t& req) {
        result r = basic_populate(req);
        r.path = path;
        r.aux_path = std::string(new_path);
        return r;
    };

    co_return co_await run_fs<result>(
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

task<result<fs_request::result>> fs_request::fsync(event_loop& loop, int fd) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_fsync(static_cast<uv_loop_t*>(loop.handle()), &req, fd, cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::fdatasync(event_loop& loop, int fd) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_fdatasync(static_cast<uv_loop_t*>(loop.handle()), &req, fd, cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::ftruncate(event_loop& loop,
                                                       int fd,
                                                       std::int64_t offset) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_ftruncate(static_cast<uv_loop_t*>(loop.handle()), &req, fd, offset, cb);
        },
        basic_populate);
}

task<result<fs_request::result>> fs_request::sendfile(event_loop& loop,
                                                      int out_fd,
                                                      int in_fd,
                                                      std::int64_t in_offset,
                                                      std::size_t length) {
    co_return co_await run_fs<result>(
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

task<result<fs_request::result>> fs_request::access(event_loop& loop,
                                                    std::string_view path,
                                                    int mode) {
    co_return co_await run_fs<result>(
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

task<result<fs_request::result>> fs_request::chmod(event_loop& loop,
                                                   std::string_view path,
                                                   int mode) {
    co_return co_await run_fs<result>(
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

task<result<fs_request::result>>
    fs_request::utime(event_loop& loop, std::string_view path, double atime, double mtime) {
    co_return co_await run_fs<result>(
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

task<result<fs_request::result>>
    fs_request::futime(event_loop& loop, int fd, double atime, double mtime) {
    co_return co_await run_fs<result>(
        loop,
        [&](uv_fs_t& req, uv_fs_cb cb) {
            return uv_fs_futime(static_cast<uv_loop_t*>(loop.handle()), &req, fd, atime, mtime, cb);
        },
        basic_populate);
}

task<result<fs_request::result>>
    fs_request::lutime(event_loop& loop, std::string_view path, double atime, double mtime) {
    co_return co_await run_fs<result>(
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

task<result<fs_request::result>> fs_request::link(event_loop& loop,
                                                  std::string_view path,
                                                  std::string_view new_path) {
    auto populate = [&](uv_fs_t& req) {
        result r = basic_populate(req);
        r.path = path;
        r.aux_path = std::string(new_path);
        return r;
    };

    co_return co_await run_fs<result>(
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
