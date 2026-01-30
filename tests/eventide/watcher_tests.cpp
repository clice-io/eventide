#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/watcher.h"

namespace eventide {

namespace {

task<error> wait_timer(timer& t) {
    auto ec = co_await t.wait();
    event_loop::current()->stop();
    co_return ec;
}

task<error> wait_idle(idle& w) {
    auto ec = co_await w.wait();
    event_loop::current()->stop();
    co_return ec;
}

}  // namespace

TEST_SUITE(watcher_io) {

TEST_CASE(timer_wait) {
    event_loop loop;

    auto timer_res = timer::create(loop);
    ASSERT_TRUE(timer_res.has_value());

    auto start_ec = timer_res->start(1, 0);
    EXPECT_FALSE(static_cast<bool>(start_ec));

    auto waiter = wait_timer(*timer_res);
    loop.schedule(waiter);
    loop.run();

    auto ec = waiter.result();
    EXPECT_FALSE(static_cast<bool>(ec));
}

TEST_CASE(idle_wait) {
    event_loop loop;

    auto idle_res = idle::create(loop);
    ASSERT_TRUE(idle_res.has_value());

    auto start_ec = idle_res->start();
    EXPECT_FALSE(static_cast<bool>(start_ec));

    auto waiter = wait_idle(*idle_res);
    loop.schedule(waiter);
    loop.run();

    auto ec = waiter.result();
    EXPECT_FALSE(static_cast<bool>(ec));

    auto stop_ec = idle_res->stop();
    EXPECT_FALSE(static_cast<bool>(stop_ec));
}

};  // TEST_SUITE(watcher_io)

}  // namespace eventide
