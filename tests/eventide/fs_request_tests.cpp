#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <BaseTsd.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/request.h"
#include "eventide/task.h"

namespace eventide {

namespace {

#ifdef _WIN32
using ssize_t = SSIZE_T;

inline int open_fd(const std::string& path) {
    return _open(path.c_str(), _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
}

inline ssize_t write_fd(int fd, const char* data, size_t len) {
    return _write(fd, data, static_cast<unsigned int>(len));
}

inline void close_fd(int fd) {
    _close(fd);
}
#else
inline int open_fd(const std::string& path) {
    return ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
}

inline ssize_t write_fd(int fd, const char* data, size_t len) {
    return ::write(fd, data, len);
}

inline void close_fd(int fd) {
    ::close(fd);
}
#endif

task<result<int>> fs_roundtrip(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "eventide-XXXXXX").string();
    auto dir_res = co_await fs::mkdtemp(loop, dir_template);
    if(!dir_res.has_value()) {
        co_return std::unexpected(dir_res.error());
    }

    std::string dir = dir_res->path;
    if(dir.empty()) {
        co_return std::unexpected(error::invalid_argument);
    }

    std::string file = (std::filesystem::path(dir) / "sample.txt").string();
    int fd = open_fd(file);
    if(fd < 0) {
        co_return std::unexpected(error::io_error);
    }

    constexpr std::string_view payload = "eventide-fs";
    if(write_fd(fd, payload.data(), payload.size()) != static_cast<ssize_t>(payload.size())) {
        close_fd(fd);
        co_return std::unexpected(error::io_error);
    }
    close_fd(fd);

    auto stat_res = co_await fs::stat(loop, file);
    if(!stat_res.has_value()) {
        co_return std::unexpected(stat_res.error());
    }

    auto dir_res2 = co_await fs::opendir(loop, dir);
    if(!dir_res2.has_value()) {
        co_return std::unexpected(dir_res2.error());
    }

    auto entries = co_await fs::readdir(loop, *dir_res2);
    if(!entries.has_value()) {
        co_await fs::closedir(loop, *dir_res2);
        co_return std::unexpected(entries.error());
    }

    bool found = false;
    for(const auto& ent: *entries) {
        if(ent.name == "sample.txt") {
            found = true;
            break;
        }
    }

    auto close_res = co_await fs::closedir(loop, *dir_res2);
    if(close_res) {
        co_return std::unexpected(close_res);
    }

    auto unlink_res = co_await fs::unlink(loop, file);
    if(!unlink_res.has_value()) {
        co_return std::unexpected(unlink_res.error());
    }

    auto rmdir_res = co_await fs::rmdir(loop, dir);
    if(!rmdir_res.has_value()) {
        co_return std::unexpected(rmdir_res.error());
    }

    co_return found ? 1 : 0;
}

task<result<int>> mkstemp_roundtrip(event_loop& loop) {
    auto file_template = (std::filesystem::temp_directory_path() / "eventide-file-XXXXXX").string();
    auto file_res = co_await fs::mkstemp(loop, file_template);
    if(!file_res.has_value()) {
        co_return std::unexpected(file_res.error());
    }

    const int fd = static_cast<int>(file_res->value);
    std::string path = file_res->path;
    if(fd >= 0) {
        close_fd(fd);
    }

    if(path.empty()) {
        co_return std::unexpected(error::invalid_argument);
    }

    auto access_res = co_await fs::access(loop, path, 0);
    if(!access_res.has_value()) {
        co_return std::unexpected(access_res.error());
    }

    auto unlink_res = co_await fs::unlink(loop, path);
    if(!unlink_res.has_value()) {
        co_return std::unexpected(unlink_res.error());
    }

    co_return 1;
}

}  // namespace

TEST_SUITE(fs_request_io) {

TEST_CASE(basic_roundtrip) {
    event_loop loop;

    auto worker = fs_roundtrip(loop);
    loop.schedule(worker);
    loop.run();

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(mkstemp_and_access) {
    event_loop loop;

    auto worker = mkstemp_roundtrip(loop);
    loop.schedule(worker);
    loop.run();

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

};  // TEST_SUITE(fs_request_io)

}  // namespace eventide
