#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/async/cancellation.h"
#include "eventide/async/fs.h"
#include "eventide/async/loop.h"
#include "eventide/async/request.h"
#include "eventide/async/sync.h"
#include "eventide/async/watcher.h"

namespace eventide {

namespace {

int uv_thread_pool_size_for_test() {
    int value = 4;
    if(const char* raw = std::getenv("UV_THREADPOOL_SIZE"); raw != nullptr) {
        int parsed = std::atoi(raw);
        if(parsed > 0) {
            value = parsed;
        }
    }

    constexpr int max_reasonable = 32;
    if(value > max_reasonable) {
        value = max_reasonable;
    }

    return value;
}

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

TEST_CASE(with_token_queue_cancel_resumes_after_cancel_call_returns) {
    event_loop loop;
    cancellation_source source;
    event start_target;
    event target_done;

    const int pool_size = uv_thread_pool_size_for_test();
    std::atomic<int> blockers_started{0};
    std::atomic<int> blockers_done{0};
    std::atomic<bool> release{false};

    int phase = 0;
    int observed_phase = 0;
    bool target_cancelled = false;

    auto blocker = [&]() -> task<> {
        auto ec = co_await queue(
            [&] {
                blockers_started.fetch_add(1, std::memory_order_relaxed);
                while(!release.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
            },
            loop);
        EXPECT_FALSE(static_cast<bool>(ec));
        blockers_done.fetch_add(1, std::memory_order_release);
    };

    auto target = [&]() -> task<> {
        co_await start_target.wait();
        auto res = co_await with_token(source.token(), queue([] {}, loop));
        target_cancelled = !res.has_value();
        observed_phase = phase;
        target_done.set();
    };

    auto canceler = [&]() -> task<> {
        while(blockers_started.load(std::memory_order_acquire) < pool_size) {
            co_await sleep(std::chrono::milliseconds{1}, loop);
        }

        start_target.set();
        co_await sleep(std::chrono::milliseconds{1}, loop);

        phase = 1;
        source.cancel();
        phase = 2;

        release.store(true, std::memory_order_release);

        co_await target_done.wait();
        while(blockers_done.load(std::memory_order_acquire) < pool_size) {
            co_await sleep(std::chrono::milliseconds{1}, loop);
        }

        loop.stop();
    };

    std::vector<task<>> blockers;
    blockers.reserve(static_cast<std::size_t>(pool_size));
    for(int i = 0; i < pool_size; ++i) {
        blockers.push_back(blocker());
    }

    auto target_task = target();
    auto cancel_task = canceler();

    for(auto& b: blockers) {
        loop.schedule(b);
    }
    loop.schedule(target_task);
    loop.schedule(cancel_task);
    loop.run();

    EXPECT_TRUE(target_cancelled);
    EXPECT_EQ(observed_phase, 2);
}

TEST_CASE(with_token_fs_cancel_resumes_after_cancel_call_returns) {
    event_loop loop;
    cancellation_source source;
    event start_target;
    event target_done;

    const int pool_size = uv_thread_pool_size_for_test();
    std::atomic<int> blockers_started{0};
    std::atomic<int> blockers_done{0};
    std::atomic<bool> release{false};

    int phase = 0;
    int observed_phase = 0;
    bool target_cancelled = false;

    auto blocker = [&]() -> task<> {
        auto ec = co_await queue(
            [&] {
                blockers_started.fetch_add(1, std::memory_order_relaxed);
                while(!release.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
            },
            loop);
        EXPECT_FALSE(static_cast<bool>(ec));
        blockers_done.fetch_add(1, std::memory_order_release);
    };

    auto target = [&]() -> task<> {
        co_await start_target.wait();
        auto res = co_await with_token(source.token(), fs::stat(".", loop));
        target_cancelled = !res.has_value();
        observed_phase = phase;
        target_done.set();
    };

    auto canceler = [&]() -> task<> {
        while(blockers_started.load(std::memory_order_acquire) < pool_size) {
            co_await sleep(std::chrono::milliseconds{1}, loop);
        }

        start_target.set();
        co_await sleep(std::chrono::milliseconds{1}, loop);

        phase = 1;
        source.cancel();
        phase = 2;

        release.store(true, std::memory_order_release);

        co_await target_done.wait();
        while(blockers_done.load(std::memory_order_acquire) < pool_size) {
            co_await sleep(std::chrono::milliseconds{1}, loop);
        }

        loop.stop();
    };

    std::vector<task<>> blockers;
    blockers.reserve(static_cast<std::size_t>(pool_size));
    for(int i = 0; i < pool_size; ++i) {
        blockers.push_back(blocker());
    }

    auto target_task = target();
    auto cancel_task = canceler();

    for(auto& b: blockers) {
        loop.schedule(b);
    }
    loop.schedule(target_task);
    loop.schedule(cancel_task);
    loop.run();

    EXPECT_TRUE(target_cancelled);
    EXPECT_EQ(observed_phase, 2);
}

};  // TEST_SUITE(cancellation)

}  // namespace

}  // namespace eventide
