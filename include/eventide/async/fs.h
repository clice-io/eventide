#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "owned.h"
#include "task.h"

namespace eventide {

class event_loop;

namespace fs {

struct result {
    std::int64_t value = 0;
    std::string path;
    std::string aux_path;
};

struct dirent {
    enum class type {
        unknown,      // type not known
        file,         // regular file
        dir,          // directory
        link,         // symlink
        fifo,         // FIFO/pipe
        socket,       // socket
        char_device,  // character device
        block_device  // block device
    };
    std::string name;
    type kind = type::unknown;
};

struct copyfile_options {
    /// Fail if destination exists.
    bool excl = false;

    /// Try to clone via copy-on-write if supported.
    bool clone = false;

    /// Force clone (may fall back to copy on failure).
    bool clone_force = false;
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

task<result, error> unlink(std::string_view path, event_loop& loop = event_loop::current());

task<result, error> mkdir(std::string_view path,
                          int mode,
                          event_loop& loop = event_loop::current());

task<result, error> stat(std::string_view path, event_loop& loop = event_loop::current());

task<result, error> copyfile(std::string_view path,
                             std::string_view new_path,
                             copyfile_options options = copyfile_options{},
                             event_loop& loop = event_loop::current());

task<result, error> mkdtemp(std::string_view tpl, event_loop& loop = event_loop::current());

task<result, error> mkstemp(std::string_view tpl, event_loop& loop = event_loop::current());

task<result, error> rmdir(std::string_view path, event_loop& loop = event_loop::current());

task<std::vector<dirent>, error> scandir(std::string_view path,
                                         event_loop& loop = event_loop::current());

task<dir_handle, error> opendir(std::string_view path, event_loop& loop = event_loop::current());

task<std::vector<dirent>, error> readdir(dir_handle& dir, event_loop& loop = event_loop::current());

task<error> closedir(dir_handle& dir, event_loop& loop = event_loop::current());

task<result, error> fstat(int fd, event_loop& loop = event_loop::current());

task<result, error> lstat(std::string_view path, event_loop& loop = event_loop::current());

task<result, error> rename(std::string_view path,
                           std::string_view new_path,
                           event_loop& loop = event_loop::current());

task<result, error> fsync(int fd, event_loop& loop = event_loop::current());

task<result, error> fdatasync(int fd, event_loop& loop = event_loop::current());

task<result, error> ftruncate(int fd,
                              std::int64_t offset,
                              event_loop& loop = event_loop::current());

task<result, error> sendfile(int out_fd,
                             int in_fd,
                             std::int64_t in_offset,
                             std::size_t length,
                             event_loop& loop = event_loop::current());

task<result, error> access(std::string_view path,
                           int mode,
                           event_loop& loop = event_loop::current());

task<result, error> chmod(std::string_view path,
                          int mode,
                          event_loop& loop = event_loop::current());

task<result, error> utime(std::string_view path,
                          double atime,
                          double mtime,
                          event_loop& loop = event_loop::current());

task<result, error>
    futime(int fd, double atime, double mtime, event_loop& loop = event_loop::current());

task<result, error> lutime(std::string_view path,
                           double atime,
                           double mtime,
                           event_loop& loop = event_loop::current());

task<result, error> link(std::string_view path,
                         std::string_view new_path,
                         event_loop& loop = event_loop::current());

}  // namespace fs

class fs_event {
public:
    fs_event() noexcept;

    fs_event(const fs_event&) = delete;
    fs_event& operator=(const fs_event&) = delete;

    fs_event(fs_event&& other) noexcept;
    fs_event& operator=(fs_event&& other) noexcept;

    ~fs_event();

    struct Self;
    Self* operator->() noexcept;

    struct watch_options {
        /// Report creation/removal events (if supported by backend).
        bool watch_entry;

        /// Use stat polling where available.
        bool stat;

        /// Recurse into subdirectories when supported.
        bool recursive;

        constexpr watch_options(bool watch_entry = false,
                                bool stat = false,
                                bool recursive = false) :
            watch_entry(watch_entry), stat(stat), recursive(recursive) {}
    };

    struct change_flags {
        /// Entry renamed or moved.
        bool rename;

        /// Entry content/metadata changed.
        bool change;

        constexpr change_flags(bool rename = false, bool change = false) :
            rename(rename), change(change) {}
    };

    struct change {
        std::string path;
        change_flags flags = {};
    };

    static result<fs_event> create(event_loop& loop = event_loop::current());

    /// Start watching the given path; flags mapped to libuv equivalents.
    error start(const char* path, watch_options options = watch_options{});

    error stop();

    /// Await a change event; delivers one pending change at a time.
    task<change, error> wait();

private:
    explicit fs_event(unique_handle<Self> self) noexcept;

    unique_handle<Self> self;
};

}  // namespace eventide
