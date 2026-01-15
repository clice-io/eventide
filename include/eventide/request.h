#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "async/loop.h"

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

    static task<std::expected<result, std::error_code>> unlink(event_loop& loop,
                                                               std::string_view path);

    static task<std::expected<result, std::error_code>> mkdir(event_loop& loop,
                                                              std::string_view path,
                                                              int mode);

    static task<std::expected<result, std::error_code>> stat(event_loop& loop,
                                                             std::string_view path);

    static task<std::expected<result, std::error_code>>
        copyfile(event_loop& loop, std::string_view path, std::string_view new_path, int flags);

    static task<std::expected<result, std::error_code>> mkdtemp(event_loop& loop,
                                                                std::string_view tpl);

    static task<std::expected<result, std::error_code>> mkstemp(event_loop& loop,
                                                                std::string_view tpl);

    static task<std::expected<result, std::error_code>> rmdir(event_loop& loop,
                                                              std::string_view path);

    static task<std::expected<std::vector<dirent>, std::error_code>> scandir(event_loop& loop,
                                                                             std::string_view path,
                                                                             int flags);

    static task<std::expected<dir_handle, std::error_code>> opendir(event_loop& loop,
                                                                    std::string_view path);

    static task<std::expected<std::vector<dirent>, std::error_code>> readdir(event_loop& loop,
                                                                             dir_handle& dir);

    static task<std::error_code> closedir(event_loop& loop, dir_handle& dir);

    static task<std::expected<result, std::error_code>> fstat(event_loop& loop, int fd);

    static task<std::expected<result, std::error_code>> lstat(event_loop& loop,
                                                              std::string_view path);

    static task<std::expected<result, std::error_code>> rename(event_loop& loop,
                                                               std::string_view path,
                                                               std::string_view new_path);

    static task<std::expected<result, std::error_code>> fsync(event_loop& loop, int fd);

    static task<std::expected<result, std::error_code>> fdatasync(event_loop& loop, int fd);

    static task<std::expected<result, std::error_code>> ftruncate(event_loop& loop,
                                                                  int fd,
                                                                  std::int64_t offset);

    static task<std::expected<result, std::error_code>> sendfile(event_loop& loop,
                                                                 int out_fd,
                                                                 int in_fd,
                                                                 std::int64_t in_offset,
                                                                 std::size_t length);

    static task<std::expected<result, std::error_code>> access(event_loop& loop,
                                                               std::string_view path,
                                                               int mode);

    static task<std::expected<result, std::error_code>> chmod(event_loop& loop,
                                                              std::string_view path,
                                                              int mode);

    static task<std::expected<result, std::error_code>>
        utime(event_loop& loop, std::string_view path, double atime, double mtime);

    static task<std::expected<result, std::error_code>>
        futime(event_loop& loop, int fd, double atime, double mtime);

    static task<std::expected<result, std::error_code>>
        lutime(event_loop& loop, std::string_view path, double atime, double mtime);

    static task<std::expected<result, std::error_code>> link(event_loop& loop,
                                                             std::string_view path,
                                                             std::string_view new_path);
};

}  // namespace eventide
