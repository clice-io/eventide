#include <expected>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <unistd.h>

#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/request.h"
#include "eventide/task.h"

namespace eventide {

namespace {

task<std::expected<int, std::error_code>> fs_roundtrip(event_loop& loop) {
    auto dir_res = co_await fs_request::mkdtemp(loop, "/tmp/eventide-XXXXXX");
    if(!dir_res.has_value()) {
        co_return std::unexpected(dir_res.error());
    }

    std::string dir = dir_res->path;
    if(dir.empty()) {
        co_return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    std::string file = dir + "/sample.txt";
    int fd = ::open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(fd < 0) {
        co_return std::unexpected(std::make_error_code(std::errc::io_error));
    }

    constexpr std::string_view payload = "eventide-fs";
    if(::write(fd, payload.data(), payload.size()) != static_cast<ssize_t>(payload.size())) {
        ::close(fd);
        co_return std::unexpected(std::make_error_code(std::errc::io_error));
    }
    ::close(fd);

    auto stat_res = co_await fs_request::stat(loop, file);
    if(!stat_res.has_value()) {
        co_return std::unexpected(stat_res.error());
    }

    auto dir_res2 = co_await fs_request::opendir(loop, dir);
    if(!dir_res2.has_value()) {
        co_return std::unexpected(dir_res2.error());
    }

    auto entries = co_await fs_request::readdir(loop, *dir_res2);
    if(!entries.has_value()) {
        co_await fs_request::closedir(loop, *dir_res2);
        co_return std::unexpected(entries.error());
    }

    bool found = false;
    for(const auto& ent: *entries) {
        if(ent.name == "sample.txt") {
            found = true;
            break;
        }
    }

    auto close_res = co_await fs_request::closedir(loop, *dir_res2);
    if(close_res) {
        co_return std::unexpected(close_res);
    }

    auto unlink_res = co_await fs_request::unlink(loop, file);
    if(!unlink_res.has_value()) {
        co_return std::unexpected(unlink_res.error());
    }

    auto rmdir_res = co_await fs_request::rmdir(loop, dir);
    if(!rmdir_res.has_value()) {
        co_return std::unexpected(rmdir_res.error());
    }

    co_return found ? 1 : 0;
}

task<std::expected<int, std::error_code>> mkstemp_roundtrip(event_loop& loop) {
    auto file_res = co_await fs_request::mkstemp(loop, "/tmp/eventide-file-XXXXXX");
    if(!file_res.has_value()) {
        co_return std::unexpected(file_res.error());
    }

    const int fd = static_cast<int>(file_res->value);
    std::string path = file_res->path;
    if(fd >= 0) {
        ::close(fd);
    }

    if(path.empty()) {
        co_return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    auto access_res = co_await fs_request::access(loop, path, 0);
    if(!access_res.has_value()) {
        co_return std::unexpected(access_res.error());
    }

    auto unlink_res = co_await fs_request::unlink(loop, path);
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
