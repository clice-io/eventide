#include <string>

#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/process.h"

namespace eventide {

namespace {

task<process::wait_result> wait_for_exit(process& proc) {
    auto status = co_await proc.wait();
    event_loop::current()->stop();
    co_return status;
}

}  // namespace

TEST_SUITE(process_io) {

TEST_CASE(spawn_wait_simple) {
    event_loop loop;

    process::options opts;
#ifdef _WIN32
    opts.file = "cmd.exe";
    opts.args = {"/c", "exit 0"};
#else
    opts.file = "/bin/sh";
    opts.args = {"-c", "true"};
#endif
    opts.streams = {process::stdio::ignore(), process::stdio::ignore(), process::stdio::ignore()};

    auto spawn_res = process::spawn(loop, opts);
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

TEST_CASE(spawn_invalid_file) {
    event_loop loop;

    process::options opts;
#ifdef _WIN32
    opts.file = "Z:\\nonexistent\\eventide-nope.exe";
#else
    opts.file = "/nonexistent/eventide-nope";
#endif

    auto spawn_res = process::spawn(loop, opts);
    EXPECT_FALSE(spawn_res.has_value());
}

};  // TEST_SUITE(process_io)

}  // namespace eventide
