#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "loop.h"

namespace eventide {

class event_loop;

using work_fn = std::function<void()>;

task<error> queue(work_fn fn, event_loop& loop = event_loop::current());

namespace fs {

struct result {
    std::int64_t value = 0;
    std::string path;
    std::string aux_path;
};

using op_result = ::eventide::result<fs::result>;

struct dirent {
    enum class type { unknown, file, dir, link, fifo, socket, char_device, block_device };
    std::string name;
    type kind = type::unknown;
};

class dir_handle {
public:
    dir_handle() = default;
    dir_handle(dir_handle&& other) noexcept;
    dir_handle& operator=(dir_handle&& other) noexcept;

    dir_handle(const dir_handle&) = delete;
    dir_handle& operator=(const dir_handle&) = delete;

    bool valid() const noexcept;
    void* native_handle() const noexcept;
    void reset() noexcept;

    static dir_handle from_native(void* ptr);

private:
    explicit dir_handle(void* ptr);

    void* dir = nullptr;
};

task<op_result> unlink(std::string_view path, event_loop& loop = event_loop::current());

task<op_result> mkdir(std::string_view path, int mode, event_loop& loop = event_loop::current());

task<op_result> stat(std::string_view path, event_loop& loop = event_loop::current());

task<op_result> copyfile(std::string_view path,
                         std::string_view new_path,
                         int flags,
                         event_loop& loop = event_loop::current());

task<op_result> mkdtemp(std::string_view tpl, event_loop& loop = event_loop::current());

task<op_result> mkstemp(std::string_view tpl, event_loop& loop = event_loop::current());

task<op_result> rmdir(std::string_view path, event_loop& loop = event_loop::current());

task<::eventide::result<std::vector<dirent>>> scandir(std::string_view path,
                                                      int flags,
                                                      event_loop& loop = event_loop::current());

task<::eventide::result<dir_handle>> opendir(std::string_view path,
                                             event_loop& loop = event_loop::current());

task<::eventide::result<std::vector<dirent>>> readdir(dir_handle& dir,
                                                      event_loop& loop = event_loop::current());

task<error> closedir(dir_handle& dir, event_loop& loop = event_loop::current());

task<op_result> fstat(int fd, event_loop& loop = event_loop::current());

task<op_result> lstat(std::string_view path, event_loop& loop = event_loop::current());

task<op_result> rename(std::string_view path,
                       std::string_view new_path,
                       event_loop& loop = event_loop::current());

task<op_result> fsync(int fd, event_loop& loop = event_loop::current());

task<op_result> fdatasync(int fd, event_loop& loop = event_loop::current());

task<op_result> ftruncate(int fd, std::int64_t offset, event_loop& loop = event_loop::current());

task<op_result> sendfile(int out_fd,
                         int in_fd,
                         std::int64_t in_offset,
                         std::size_t length,
                         event_loop& loop = event_loop::current());

task<op_result> access(std::string_view path, int mode, event_loop& loop = event_loop::current());

task<op_result> chmod(std::string_view path, int mode, event_loop& loop = event_loop::current());

task<op_result> utime(std::string_view path,
                      double atime,
                      double mtime,
                      event_loop& loop = event_loop::current());

task<op_result>
    futime(int fd, double atime, double mtime, event_loop& loop = event_loop::current());

task<op_result> lutime(std::string_view path,
                       double atime,
                       double mtime,
                       event_loop& loop = event_loop::current());

task<op_result> link(std::string_view path,
                     std::string_view new_path,
                     event_loop& loop = event_loop::current());

}  // namespace fs

}  // namespace eventide
