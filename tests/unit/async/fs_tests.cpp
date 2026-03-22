#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#endif

#include "../common/fd_helpers.h"
#include "eventide/zest/zest.h"
#include "eventide/async/async.h"

namespace eventide {

using test::close_fd;
using test::write_fd;

namespace {

#ifdef _WIN32
inline int open_fd(const std::string& path) {
    int fd = -1;
    if(_sopen_s(&fd,
                path.c_str(),
                _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY,
                _SH_DENYNO,
                _S_IREAD | _S_IWRITE) != 0) {
        return -1;
    }
    return fd;
}
#else
inline int open_fd(const std::string& path) {
    return ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
}
#endif

task<int, error> fs_roundtrip(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "eventide-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();
    if(dir.empty()) {
        co_await fail(error::invalid_argument);
    }

    std::string file = (std::filesystem::path(dir) / "sample.txt").string();
    int fd = open_fd(file);
    if(fd < 0) {
        co_await fail(error::io_error);
    }

    constexpr std::string_view payload = "eventide-fs";
    if(write_fd(fd, payload.data(), payload.size()) != static_cast<ssize_t>(payload.size())) {
        close_fd(fd);
        co_await fail(error::io_error);
    }
    close_fd(fd);

    co_await fs::stat(file, loop).or_fail();

    auto dir_handle = co_await fs::opendir(dir, loop).or_fail();
    auto entries = co_await fs::readdir(dir_handle, loop);
    if(!entries.has_value()) {
        co_await fs::closedir(dir_handle, loop);
        co_await fail(entries.error());
    }

    bool found = false;
    for(const auto& ent: *entries) {
        if(ent.name == "sample.txt") {
            found = true;
            break;
        }
    }

    co_await fs::closedir(dir_handle, loop).or_fail();
    co_await fs::unlink(file, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return found ? 1 : 0;
}

task<int, error> mkstemp_roundtrip(event_loop& loop) {
    auto file_template = (std::filesystem::temp_directory_path() / "eventide-file-XXXXXX").string();
    auto file_info = co_await fs::mkstemp(file_template, loop).or_fail();
    const int fd = file_info.fd;
    std::string path = std::move(file_info.path);
    if(fd >= 0) {
        close_fd(fd);
    }

    if(path.empty()) {
        co_await fail(error::invalid_argument);
    }

    co_await fs::access(path, 0, loop).or_fail();
    co_await fs::unlink(path, loop).or_fail();

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
