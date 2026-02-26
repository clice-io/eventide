#include <chrono>

#include "eventide/zest/zest.h"
#include "eventide/async/cancellation.h"
#include "eventide/async/loop.h"
#include "eventide/async/sync.h"
#include "eventide/async/watcher.h"

namespace eventide {

namespace {

TEST_SUITE(cancellation) {

TEST_CASE(with_token_pass_through_value) {
    cancellation_source source;

    auto worker = []() -> task<int> {
        co_return 42;
    };

    auto [result] = run(with_token(source.token(), worker()));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_CASE(with_token_pre_cancel_does_not_start_child) {
    cancellation_source source;
    source.cancel();

    int started = 0;
    auto worker = [&]() -> task<int> {
        started += 1;
        co_return 1;
    };

    auto [result] = run(with_token(source.token(), worker()));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(started, 0);
}

TEST_CASE(with_token_cancel_in_flight) {
    event_loop loop;
    cancellation_source source;
    event gate;
    int started = 0;
    int finished = 0;

    auto worker = [&]() -> task<int> {
        started += 1;
        co_await gate.wait();
        finished += 1;
        co_return 7;
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(std::chrono::milliseconds{1}, loop);
        source.cancel();
    };

    auto releaser = [&]() -> task<> {
        co_await sleep(std::chrono::milliseconds{2}, loop);
        gate.set();
    };

    auto guarded_task = with_token(source.token(), worker());
    auto cancel_task = canceler();
    auto release_task = releaser();

    loop.schedule(guarded_task);
    loop.schedule(cancel_task);
    loop.schedule(release_task);
    loop.run();

    auto result = guarded_task.value();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(started, 1);
    EXPECT_EQ(finished, 0);
}

TEST_CASE(source_destructor_cancels_tokens) {
    cancellation_token token;
    {
        cancellation_source source;
        token = source.token();
        EXPECT_FALSE(token.cancelled());
    }

    EXPECT_TRUE(token.cancelled());
}

TEST_CASE(token_copies_share_state) {
    cancellation_source source;
    auto token_a = source.token();
    auto token_b = token_a;

    EXPECT_FALSE(token_a.cancelled());
    EXPECT_FALSE(token_b.cancelled());

    source.cancel();
    EXPECT_TRUE(token_a.cancelled());
    EXPECT_TRUE(token_b.cancelled());
}

};  // TEST_SUITE(cancellation)

}  // namespace

}  // namespace eventide
