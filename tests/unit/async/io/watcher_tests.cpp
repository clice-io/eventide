#include <chrono>
#include <csignal>

#include "../loop_fixture.h"
#include "kota/zest/zest.h"
#include "kota/async/vocab/cancellation.h"

namespace kota {

namespace {

task<> wait_timer(timer& t) {
    co_await t.wait();
    event_loop::current().stop();
    co_return;
}

task<> wait_idle(idle& w) {
    co_await w.wait();
    event_loop::current().stop();
    co_return;
}

task<> wait_sleep(event_loop& loop) {
    co_await sleep(1, loop);
    event_loop::current().stop();
    co_return;
}

task<> wait_prepare(prepare& w) {
    co_await w.wait();
    event_loop::current().stop();
    co_return;
}

task<> wait_check(check& w) {
    co_await w.wait();
    event_loop::current().stop();
    co_return;
}

task<> wait_timer_twice(timer& t) {
    co_await t.wait();
    co_await t.wait();
    t.stop();
    event_loop::current().stop();
    co_return;
}

}  // namespace

TEST_SUITE(watcher_io, loop_fixture) {

TEST_CASE(timer_wait) {
    auto t = timer::create(loop);
    t.start(std::chrono::milliseconds{1}, std::chrono::milliseconds{0});

    auto waiter = wait_timer(t);
    schedule_all(waiter);
}

TEST_CASE(idle_wait) {
    auto w = idle::create(loop);
    w.start();

    auto waiter = wait_idle(w);
    schedule_all(waiter);

    w.stop();
}

TEST_CASE(sleep_once) {
    auto sleeper = wait_sleep(loop);
    schedule_all(sleeper);
}

TEST_CASE(timer_repeat_twice) {
    auto t = timer::create(loop);
    t.start(std::chrono::milliseconds{1}, std::chrono::milliseconds{1});

    auto waiter = wait_timer_twice(t);
    schedule_all(waiter);
}

TEST_CASE(prepare_wait) {
    auto w = prepare::create(loop);
    w.start();

    auto waiter = wait_prepare(w);
    schedule_all(waiter);

    w.stop();
}

TEST_CASE(check_wait) {
    auto w = check::create(loop);
    w.start();

    auto waiter = wait_check(w);
    schedule_all(waiter);

    w.stop();
}

TEST_CASE(timer_wait_cancel) {
    auto t = timer::create(loop);
    t.start(std::chrono::milliseconds{60000}, std::chrono::milliseconds{0});

    cancellation_source source;

    auto worker = [&]() -> task<void, void, cancellation> {
        co_await t.wait();
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(10, loop);
        source.cancel();
    };

    auto guarded = with_token(worker(), source.token());
    auto cancel_task = canceler();
    schedule_all(guarded, cancel_task);

    EXPECT_TRUE(guarded.result().is_cancelled());
    t.stop();
}

TEST_CASE(sleep_cancel) {
    cancellation_source source;

    auto worker = [&]() -> task<void, void, cancellation> {
        co_await sleep(60000, loop);
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(10, loop);
        source.cancel();
    };

    auto guarded = with_token(worker(), source.token());
    auto cancel_task = canceler();
    schedule_all(guarded, cancel_task);

    EXPECT_TRUE(guarded.result().is_cancelled());
}

TEST_CASE(idle_wait_cancel) {
    cancellation_source source;

    auto w = idle::create(loop);
    w.start();

    int ticks = 0;
    auto worker = [&]() -> task<void, void, cancellation> {
        while(true) {
            co_await w.wait();
            ++ticks;
        }
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(10, loop);
        source.cancel();
    };

    auto guarded = with_token(worker(), source.token());
    auto cancel_task = canceler();
    schedule_all(guarded, cancel_task);

    EXPECT_TRUE(guarded.result().is_cancelled());
    EXPECT_GT(ticks, 0);
    w.stop();
}

TEST_CASE(prepare_wait_cancel) {
    cancellation_source source;

    auto w = prepare::create(loop);
    w.start();

    int ticks = 0;
    auto worker = [&]() -> task<void, void, cancellation> {
        while(true) {
            co_await w.wait();
            ++ticks;
        }
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(10, loop);
        source.cancel();
    };

    auto guarded = with_token(worker(), source.token());
    auto cancel_task = canceler();
    schedule_all(guarded, cancel_task);

    EXPECT_TRUE(guarded.result().is_cancelled());
    EXPECT_GT(ticks, 0);
    w.stop();
}

TEST_CASE(check_wait_cancel) {
    cancellation_source source;

    auto w = check::create(loop);
    w.start();

    int ticks = 0;
    auto worker = [&]() -> task<void, void, cancellation> {
        while(true) {
            co_await w.wait();
            ++ticks;
        }
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(10, loop);
        source.cancel();
    };

    auto guarded = with_token(worker(), source.token());
    auto cancel_task = canceler();
    schedule_all(guarded, cancel_task);

    EXPECT_TRUE(guarded.result().is_cancelled());
    EXPECT_GT(ticks, 0);
    w.stop();
}

#ifndef _WIN32
TEST_CASE(signal_wait_cancel) {
    cancellation_source source;

    auto sig = signal::create(loop);
    ASSERT_TRUE(sig.has_value());
    ASSERT_FALSE(sig->start(SIGUSR1).has_error());

    auto worker = [&]() -> task<void, error, cancellation> {
        co_await sig->wait();
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(10, loop);
        source.cancel();
    };

    auto guarded = with_token(worker(), source.token());
    auto cancel_task = canceler();
    schedule_all(guarded, cancel_task);

    EXPECT_TRUE(guarded.result().is_cancelled());
    sig->stop();
}
#endif

};  // TEST_SUITE(watcher_io)

}  // namespace kota
