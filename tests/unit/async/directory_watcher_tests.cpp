#include <fcntl.h>
#include <filesystem>
#include <string>
#include <variant>

#include "loop_fixture.h"
#include "kota/zest/zest.h"
#include "kota/async/io/directory_watcher.h"

namespace kota {

namespace {

task<std::vector<directory_watcher::change>, error> next_or_timeout(directory_watcher& w,
                                                                    event_loop& loop,
                                                                    int timeout_ms = 10000) {
    auto do_timeout = [&]() -> task<std::vector<directory_watcher::change>, error> {
        co_await sleep(timeout_ms, loop);
        co_return std::vector<directory_watcher::change>{};
    };

    auto result = co_await when_any(w.next(), do_timeout());
    if(result.has_error()) {
        co_await fail(result.error());
    }

    co_return std::visit(
        [](auto&& v) -> std::vector<directory_watcher::change> { return std::move(v); },
        std::move(*result));
}

task<int, error> watch_file_create(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    std::string file = (std::filesystem::path(dir) / "test.txt").string();
    int fd = co_await fs::open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher, loop).or_fail();

    bool found = false;
    for(const auto& c: changes) {
        if(c.type == directory_watcher::effect::create) {
            found = true;
        }
    }

    watcher->close();
    co_await fs::unlink(file, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return found ? 1 : 0;
}

task<int, error> watch_file_modify(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    std::string file = (std::filesystem::path(dir) / "modify.txt").string();
    int fd = co_await fs::open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    fd = co_await fs::open(file, O_WRONLY, 0, loop).or_fail();
    constexpr std::string_view payload = "hello";
    co_await fs::write(fd, std::span<const char>(payload.data(), payload.size()), -1, loop)
        .or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher, loop).or_fail();

    bool found = false;
    for(const auto& c: changes) {
        if(c.type == directory_watcher::effect::modify) {
            found = true;
        }
    }

    watcher->close();
    co_await fs::unlink(file, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return found ? 1 : 0;
}

task<int, error> watch_file_delete(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    std::string file = (std::filesystem::path(dir) / "delete.txt").string();
    int fd = co_await fs::open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    co_await fs::unlink(file, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher, loop).or_fail();

    bool found = false;
    for(const auto& c: changes) {
        if(c.type == directory_watcher::effect::destroy) {
            found = true;
        }
    }

    watcher->close();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return found ? 1 : 0;
}

task<int, error> watch_file_rename(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    std::string src = (std::filesystem::path(dir) / "before.txt").string();
    std::string dst = (std::filesystem::path(dir) / "after.txt").string();

    int fd = co_await fs::open(src, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    co_await fs::rename(src, dst, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher, loop).or_fail();

    bool found_relevant = false;
    for(const auto& c: changes) {
        if(c.type == directory_watcher::effect::rename ||
           c.type == directory_watcher::effect::create ||
           c.type == directory_watcher::effect::destroy) {
            found_relevant = true;
        }
    }

    watcher->close();
    co_await fs::unlink(dst, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return found_relevant ? 1 : 0;
}

task<int, error> watch_nonexistent_path(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    auto result = directory_watcher::create(dir, {}, loop);
    if(result.has_value()) {
        co_return 0;
    }

    co_return result.error() == error::no_such_file_or_directory ? 1 : 0;
}

task<int, error> watch_close_then_next(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    watcher->close();

    auto result = co_await watcher->next();

    co_await fs::rmdir(dir, loop).or_fail();

    if(!result.has_error()) {
        co_return 0;
    }

    co_return result.error() == error::invalid_argument ? 1 : 0;
}

task<int, error> watch_multiple_next_calls(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    std::string file1 = (std::filesystem::path(dir) / "a.txt").string();
    int fd = co_await fs::open(file1, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto batch1 = co_await next_or_timeout(*watcher, loop).or_fail();
    if(batch1.empty()) {
        co_return 0;
    }

    std::string file2 = (std::filesystem::path(dir) / "b.txt").string();
    fd = co_await fs::open(file2, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto batch2 = co_await next_or_timeout(*watcher, loop).or_fail();
    if(batch2.empty()) {
        co_return 0;
    }

    watcher->close();
    co_await fs::unlink(file1, loop).or_fail();
    co_await fs::unlink(file2, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return 1;
}

task<int, error> watch_debounce_batching(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{200}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    std::string file1 = (std::filesystem::path(dir) / "x.txt").string();
    std::string file2 = (std::filesystem::path(dir) / "y.txt").string();
    std::string file3 = (std::filesystem::path(dir) / "z.txt").string();

    int fd = co_await fs::open(file1, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();
    fd = co_await fs::open(file2, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();
    fd = co_await fs::open(file3, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher, loop).or_fail();

    watcher->close();
    co_await fs::unlink(file1, loop).or_fail();
    co_await fs::unlink(file2, loop).or_fail();
    co_await fs::unlink(file3, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return changes.size() >= 1 ? 1 : 0;
}

task<int, error> watch_subdirectory_changes(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    std::string subdir = (std::filesystem::path(dir) / "sub").string();
    co_await fs::mkdir(subdir, 0755, loop).or_fail();

    co_await sleep(500, loop);

    std::string file = (std::filesystem::path(subdir) / "nested.txt").string();
    int fd = co_await fs::open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher, loop).or_fail();

    bool found_subdir_event = false;
    for(const auto& c: changes) {
        if(c.path.find("sub") != std::string::npos) {
            found_subdir_event = true;
        }
    }

    watcher->close();
    co_await fs::unlink(file, loop).or_fail();
    co_await fs::rmdir(subdir, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return found_subdir_event ? 1 : 0;
}

task<int, error> watch_non_recursive(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    std::string subdir = (std::filesystem::path(dir) / "child").string();
    co_await fs::mkdir(subdir, 0755, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}, false},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    co_await sleep(500, loop);

    std::string file = (std::filesystem::path(dir) / "top.txt").string();
    int fd = co_await fs::open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher, loop).or_fail();

    bool found_top = false;
    for(const auto& c: changes) {
        if(c.type == directory_watcher::effect::create) {
            found_top = true;
        }
    }

    watcher->close();
    co_await fs::unlink(file, loop).or_fail();
    co_await fs::rmdir(subdir, loop).or_fail();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return found_top ? 1 : 0;
}

task<int, error> watch_move_assignment(event_loop& loop) {
    auto dir_template1 = (std::filesystem::temp_directory_path() / "kotatsu-dw1-XXXXXX").string();
    auto dir_template2 = (std::filesystem::temp_directory_path() / "kotatsu-dw2-XXXXXX").string();
    std::string dir1 = co_await fs::mkdtemp(dir_template1, loop).or_fail();
    std::string dir2 = co_await fs::mkdtemp(dir_template2, loop).or_fail();

    auto watcher_a =
        directory_watcher::create(dir1,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher_a.has_value()) {
        co_await fail(watcher_a.error());
    }

    auto watcher_b =
        directory_watcher::create(dir2,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher_b.has_value()) {
        co_await fail(watcher_b.error());
    }

    *watcher_a = std::move(*watcher_b);

    co_await sleep(500, loop);

    std::string file = (std::filesystem::path(dir2) / "moved.txt").string();
    int fd = co_await fs::open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644, loop).or_fail();
    co_await fs::close(fd, loop).or_fail();

    auto changes = co_await next_or_timeout(*watcher_a, loop).or_fail();

    bool found = false;
    for(const auto& c: changes) {
        if(c.type == directory_watcher::effect::create) {
            found = true;
        }
    }

    watcher_a->close();
    co_await fs::unlink(file, loop).or_fail();
    co_await fs::rmdir(dir1, loop).or_fail();
    co_await fs::rmdir(dir2, loop).or_fail();

    co_return found ? 1 : 0;
}

task<int, error> watch_destructor_cleanup(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    {
        auto watcher =
            directory_watcher::create(dir,
                                      directory_watcher::options{std::chrono::milliseconds{50}},
                                      loop);
        if(!watcher.has_value()) {
            co_await fail(watcher.error());
        }
    }

    co_await fs::rmdir(dir, loop).or_fail();

    co_return 1;
}

task<int, error> watch_double_close(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    auto watcher =
        directory_watcher::create(dir,
                                  directory_watcher::options{std::chrono::milliseconds{50}},
                                  loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    watcher->close();
    watcher->close();

    co_await fs::rmdir(dir, loop).or_fail();

    co_return 1;
}

task<int, error> watch_default_options(event_loop& loop) {
    auto dir_template = (std::filesystem::temp_directory_path() / "kotatsu-dw-XXXXXX").string();
    std::string dir = co_await fs::mkdtemp(dir_template, loop).or_fail();

    auto watcher = directory_watcher::create(dir, {}, loop);
    if(!watcher.has_value()) {
        co_await fail(watcher.error());
    }

    watcher->close();
    co_await fs::rmdir(dir, loop).or_fail();

    co_return 1;
}

}  // namespace

TEST_SUITE(directory_watcher_io, loop_fixture) {

TEST_CASE(detect_file_creation) {
    auto worker = watch_file_create(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(detect_file_modification) {
    auto worker = watch_file_modify(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(detect_file_deletion) {
    auto worker = watch_file_delete(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(detect_file_rename) {
    auto worker = watch_file_rename(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(error_on_nonexistent_path) {
    auto worker = watch_nonexistent_path(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(close_then_next_returns_error) {
    auto worker = watch_close_then_next(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(multiple_next_calls) {
    auto worker = watch_multiple_next_calls(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(debounce_batches_events) {
    auto worker = watch_debounce_batching(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(recursive_subdirectory_events) {
    auto worker = watch_subdirectory_changes(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(non_recursive_watches_top_level) {
    auto worker = watch_non_recursive(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(move_assignment_closes_old_watcher) {
    auto worker = watch_move_assignment(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(destructor_cleans_up_without_close) {
    auto worker = watch_destructor_cleanup(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(double_close_is_safe) {
    auto worker = watch_double_close(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

TEST_CASE(create_with_default_options) {
    auto worker = watch_default_options(loop);
    schedule_all(worker);

    auto result = worker.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}

};  // TEST_SUITE(directory_watcher_io)

}  // namespace kota
