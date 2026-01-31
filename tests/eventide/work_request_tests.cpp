#include <atomic>

#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/request.h"
#include "eventide/task.h"

namespace eventide {

namespace {

task<error> wait_work(event_loop& loop, std::atomic<int>& flag) {
    auto ec = co_await queue(loop, [&]() { flag.fetch_add(1); });
    event_loop::current()->stop();
    co_return ec;
}

task<error>
    wait_work_target(event_loop& loop, std::atomic<int>& flag, std::atomic<int>& done, int target) {
    auto ec = co_await queue(loop, [&]() { flag.fetch_add(1); });
    if(done.fetch_add(1) + 1 == target) {
        event_loop::current()->stop();
    }
    co_return ec;
}

}  // namespace

TEST_SUITE(work_request_io) {

TEST_CASE(queue_runs) {
    event_loop loop;
    std::atomic<int> flag{0};

    auto worker = wait_work(loop, flag);
    loop.schedule(worker);
    loop.run();

    auto ec = worker.result();
    EXPECT_FALSE(static_cast<bool>(ec));
    EXPECT_EQ(flag.load(), 1);
}

TEST_CASE(queue_runs_twice) {
    event_loop loop;
    std::atomic<int> flag{0};
    std::atomic<int> done{0};

    auto first = wait_work_target(loop, flag, done, 2);
    auto second = wait_work_target(loop, flag, done, 2);

    loop.schedule(first);
    loop.schedule(second);
    loop.run();

    auto ec1 = first.result();
    auto ec2 = second.result();
    EXPECT_FALSE(static_cast<bool>(ec1));
    EXPECT_FALSE(static_cast<bool>(ec2));
    EXPECT_EQ(flag.load(), 2);
}

};  // TEST_SUITE(work_request_io)

}  // namespace eventide
