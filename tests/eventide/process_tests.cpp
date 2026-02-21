#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "zest/macro.h"
#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/process.h"

namespace eventide {

namespace {

task<process::wait_result> wait_for_exit(process& proc) {
    auto status = co_await proc.wait();
    event_loop::current().stop();
    co_return status;
}

}  // namespace

TEST_SUITE(process_io) {

TEST_CASE(spawn_wait_simple) {
    event_loop loop;

    process::options opts;
#ifdef _WIN32
    opts.file = "cmd.exe";
    opts.args = {opts.file, "/c", "exit 0"};
#else
    opts.file = "/bin/sh";
    opts.args = {opts.file, "-c", "true"};
#endif
    opts.streams = {process::stdio::ignore(), process::stdio::ignore(), process::stdio::ignore()};

    auto spawn_res = process::spawn(opts, loop);
    ASSERT_TRUE(spawn_res.has_value());

    EXPECT_TRUE(spawn_res->proc.pid() > 0);

    auto worker = wait_for_exit(spawn_res->proc);

    loop.schedule(worker);
    loop.run();

    auto status = worker.result();
    EXPECT_TRUE(status.has_value());
    EXPECT_EQ(status->status, 0);
    EXPECT_EQ(status->term_signal, 0);
}

TEST_CASE(spawn_pipe_stdout) {
    event_loop loop;

    process::options opts;
#ifdef _WIN32
    opts.file = "cmd.exe";
    opts.args = {opts.file, "/c", "echo eventide-stdout"};
    const std::string expected = "eventide-stdout\r\n";
#else
    opts.file = "/bin/sh";
    opts.args = {opts.file, "-c", "printf 'eventide-stdout'"};
    const std::string expected = "eventide-stdout";
#endif
    opts.streams = {process::stdio::ignore(),
                    process::stdio::pipe(false, true),
                    process::stdio::ignore()};

    auto spawn_res = process::spawn(opts, loop);
    ASSERT_TRUE(spawn_res.has_value());

    auto capture_stdout = [&]() -> task<void> {
        auto stdout_out = co_await spawn_res->stdout_pipe.read();
        auto status = co_await spawn_res->proc.wait();

        EXPECT_TRUE(status.has_value());
        if(status.has_value()) {
            EXPECT_EQ(status->status, 0);
        }

        EXPECT_TRUE(stdout_out.has_value());
        if(stdout_out.has_value()) {
            EXPECT_EQ(*stdout_out, expected);
        }

        event_loop::current().stop();
    };

    loop.schedule(capture_stdout());
    loop.run();
}

TEST_CASE(spawn_pipe_stdin_stdout) {
    event_loop loop;

    process::options opts;
#ifdef _WIN32
    opts.file = "cmd.exe";
    opts.args = {opts.file, "/c", "more"};
#else
    opts.file = "/bin/cat";
    opts.args = {opts.file};
#endif
    opts.streams = {process::stdio::pipe(true, false),
                    process::stdio::pipe(false, true),
                    process::stdio::ignore()};

    const std::string payload = "eventide-stdin-payload\n";

    auto spawn_res = process::spawn(opts, loop);
    ASSERT_TRUE(spawn_res.has_value());

    auto write_stdin_capture_stdout = [&]() -> task<void> {
        std::span<const char> data(payload.data(), payload.size());
        auto write_err = co_await spawn_res->stdin_pipe.write(data);
        EXPECT_FALSE(write_err.has_error());

        spawn_res->stdin_pipe = pipe{};

        auto stdout_out = co_await spawn_res->stdout_pipe.read();
        auto status = co_await spawn_res->proc.wait();

        EXPECT_TRUE(stdout_out.has_value());
        EXPECT_TRUE(status.has_value());
        if(status.has_value()) {
            EXPECT_EQ(status->status, 0);
        }

        auto trim_newlines = [](std::string value) {
            while(!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                value.pop_back();
            }
            return value;
        };

        if(stdout_out.has_value()) {
            EXPECT_EQ(trim_newlines(*stdout_out), trim_newlines(payload));
        }

        event_loop::current().stop();
    };

    loop.schedule(write_stdin_capture_stdout());
    loop.run();
}

TEST_CASE(spawn_pipe_stderr) {
    event_loop loop;

    process::options opts;
#ifdef _WIN32
    opts.file = "cmd.exe";
    opts.args = {opts.file, "/c", "echo eventide-stderr 1>&2"};
#else
    opts.file = "/bin/sh";
    opts.args = {opts.file, "-c", "printf 'eventide-stderr' 1>&2"};
#endif
    opts.streams = {process::stdio::ignore(),
                    process::stdio::pipe(false, true),
                    process::stdio::pipe(false, true)};

    auto spawn_res = process::spawn(opts, loop);
    ASSERT_TRUE(spawn_res.has_value());

    auto capture_stdout_stderr = [&]() -> task<void> {
        auto stdout_out = co_await spawn_res->stdout_pipe.read();
        auto stderr_out = co_await spawn_res->stderr_pipe.read();
        auto status = co_await spawn_res->proc.wait();

        EXPECT_TRUE(status.has_value());
        if(status.has_value()) {
            EXPECT_EQ(status->status, 0);
        }

        EXPECT_TRUE(!stdout_out.has_value());
        EXPECT_TRUE(stderr_out.has_value());

        if(stderr_out.has_value()) {
            EXPECT_TRUE(stderr_out->find("eventide-stderr") != std::string::npos);
        }

        event_loop::current().stop();
    };

    loop.schedule(capture_stdout_stderr());
    loop.run();
}

TEST_CASE(spawn_invalid_file) {
    event_loop loop;

    process::options opts;
#ifdef _WIN32
    opts.file = "Z:\\nonexistent\\eventide-nope.exe";
#else
    opts.file = "/nonexistent/eventide-nope";
#endif

    auto spawn_res = process::spawn(opts, loop);
    EXPECT_FALSE(spawn_res.has_value());
}

};  // TEST_SUITE(process_io)

}  // namespace eventide
