#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "error.h"
#include "loop.h"

namespace eventide {

class event_loop;

template <typename Tag>
struct awaiter;

class work_request {
public:
    using work_fn = std::function<void()>;

    static task<std::error_code> queue(event_loop& loop, work_fn fn);
};

class fs_request {
public:
    struct result {
        std::int64_t value = 0;
        std::string path;
        std::string aux_path;
    };

    using op_result = ::eventide::result<fs_request::result>;

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

    private:
        friend class fs_request;
        explicit dir_handle(void* ptr);

        void* dir = nullptr;
    };

    static task<op_result> unlink(event_loop& loop, std::string_view path);

    static task<op_result> mkdir(event_loop& loop, std::string_view path, int mode);

    static task<op_result> stat(event_loop& loop, std::string_view path);

    static task<op_result>
        copyfile(event_loop& loop, std::string_view path, std::string_view new_path, int flags);

    static task<op_result> mkdtemp(event_loop& loop, std::string_view tpl);

    static task<op_result> mkstemp(event_loop& loop, std::string_view tpl);

    static task<op_result> rmdir(event_loop& loop, std::string_view path);

    static task<::eventide::result<std::vector<dirent>>> scandir(event_loop& loop,
                                                                 std::string_view path,
                                                                 int flags);

    static task<::eventide::result<dir_handle>> opendir(event_loop& loop, std::string_view path);

    static task<::eventide::result<std::vector<dirent>>> readdir(event_loop& loop, dir_handle& dir);

    static task<std::error_code> closedir(event_loop& loop, dir_handle& dir);

    static task<op_result> fstat(event_loop& loop, int fd);

    static task<op_result> lstat(event_loop& loop, std::string_view path);

    static task<op_result> rename(event_loop& loop,
                                  std::string_view path,
                                  std::string_view new_path);

    static task<op_result> fsync(event_loop& loop, int fd);

    static task<op_result> fdatasync(event_loop& loop, int fd);

    static task<op_result> ftruncate(event_loop& loop, int fd, std::int64_t offset);

    static task<op_result> sendfile(event_loop& loop,
                                    int out_fd,
                                    int in_fd,
                                    std::int64_t in_offset,
                                    std::size_t length);

    static task<op_result> access(event_loop& loop, std::string_view path, int mode);

    static task<op_result> chmod(event_loop& loop, std::string_view path, int mode);

    static task<op_result>
        utime(event_loop& loop, std::string_view path, double atime, double mtime);

    static task<op_result> futime(event_loop& loop, int fd, double atime, double mtime);

    static task<op_result>
        lutime(event_loop& loop, std::string_view path, double atime, double mtime);

    static task<op_result> link(event_loop& loop, std::string_view path, std::string_view new_path);
};

}  // namespace eventide
