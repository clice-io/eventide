#include <stdexcept>
#include <vector>

#include "loop_fixture.h"
#include "kota/zest/macro.h"
#include "kota/zest/zest.h"
#include "kota/support/config.h"
#include "kota/async/async.h"

namespace kota {

namespace {

TEST_SUITE(task) {

TEST_CASE(task_await) {
    static auto foo = []() -> task<int> {
        co_return 1;
    };

    static auto foo1 = []() -> task<int> {
        co_return co_await foo() + 1;
    };

    static auto foo2 = []() -> task<int> {
        auto res = co_await foo();
        auto res1 = co_await foo1();
        co_return res + res1;
    };

// Visual Studio issue:
// https://developercommunity.visualstudio.com/t/Unable-to-destroy-C20-coroutine-in-fin/10657377
#if !KOTA_WORKAROUND_MSVC_COROUTINE_ASAN_UAF
    {
        event_loop loop;
        loop.schedule(foo());
        loop.run();
    }
#endif

    {
        auto [res] = run(foo());
        EXPECT_EQ(res, 1);
    }

    {
        auto [res, res1] = run(foo(), foo1());
        EXPECT_EQ(res, 1);
        EXPECT_EQ(res1, 2);
    }

    {
        auto [res, res1, res2] = run(foo(), foo1(), foo2());
        EXPECT_EQ(res, 1);
        EXPECT_EQ(res1, 2);
        EXPECT_EQ(res2, 3);
    }
}

TEST_CASE(up_cancel) {
    static auto bar = [](int& x) -> task<int> {
        x += 1;
        co_return 1;
    };

    {
        int x = 0;
        auto task = bar(x);
        task->cancel();
        run(task);
        EXPECT_TRUE(task->is_cancelled());
        EXPECT_EQ(x, 0);
    }
}

TEST_CASE(down_cancel) {
    static auto bar1 = [](int& x) -> task<> {
        x += 1;
        co_await cancel();
    };

    static auto bar2 = [](int& x) -> task<> {
        co_await bar1(x);
        x += 1;
    };

    {
        int x = 0;
        auto task = bar2(x);
        run(task);
        EXPECT_TRUE(task->is_cancelled());
        EXPECT_EQ(x, 1);
    }

    static auto bar3 = [](int& x) -> task<bool> {
        auto res = co_await bar1(x).catch_cancel();
        x += 1;
        co_return res.has_value();
    };

    {
        int x = 0;
        auto task = bar3(x);
        run(task);
        EXPECT_TRUE(task->is_finished());
        EXPECT_FALSE(task.result());
        EXPECT_EQ(x, 2);
    }
}

#if KOTA_ENABLE_EXCEPTIONS
TEST_CASE(exception_propagation) {
    auto bar1 = []() -> task<> {
        throw std::runtime_error("Test exception");
        co_return;
    };

    auto bar2 = [&]() -> task<> {
        co_return co_await bar1();
    };

    EXPECT_THROWS(run(bar1()));
    EXPECT_THROWS(run(bar2()));
}

TEST_CASE(or_fail_rethrows_child_exception) {
    auto child = []() -> task<int, error> {
        throw std::runtime_error("or_fail child exception");
        co_return 0;
    };

    auto parent = [&]() -> task<int, error> {
        co_return co_await child().or_fail();
    };

    EXPECT_THROWS(run(parent()));
}
#endif

TEST_CASE(dump_dot_basic) {
    bool checked = false;

    auto inner = []() -> task<int> {
        co_return 42;
    };

    auto outer = [&]() -> task<> {
        auto t = inner();
        auto* node = t.operator->();
        auto dot = dump_dot(*node);
        EXPECT_TRUE(!dot.empty());
        EXPECT_TRUE(dot.find("digraph") != std::string::npos);
        EXPECT_TRUE(dot.find("Task") != std::string::npos);
        checked = true;
        co_await std::move(t);
    };

    auto t = outer();
    event_loop loop;
    loop.schedule(t);
    loop.run();
    EXPECT_TRUE(checked);
}

};  // TEST_SUITE(task)

TEST_SUITE(yield, loop_fixture) {

// yield() resumes on the NEXT loop iteration: every deferred resume produced
// in the current iteration (here: the event waiter woken by set()) runs
// strictly before the yielded task continues. This is the hand-over
// guarantee that debounced-cancellation patterns rely on.
TEST_CASE(runs_after_current_drain) {
    event ev;
    std::vector<int> order;

    auto waiter = [&]() -> task<> {
        co_await ev.wait();
        order.push_back(1);
    };

    auto driver = [&]() -> task<> {
        co_await sleep(1, loop);
        ev.set();
        co_await yield(loop);
        order.push_back(2);
    };

    auto w = waiter();
    auto d = driver();
    schedule_all(w, d);

    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

// A task suspended on yield() can be cancelled; the queued completion
// delivers the cancellation on the next iteration.
TEST_CASE(cancel_while_suspended) {
    async_node* worker_node = nullptr;
    bool resumed = false;

    auto worker = [&]() -> task<> {
        co_await yield(loop);
        resumed = true;
    };

    auto canceler = [&]() -> task<> {
        worker_node->cancel();
        co_return;
    };

    auto w = worker();
    auto c = canceler();
    worker_node = w.operator->();
    schedule_all(w, c);

    EXPECT_TRUE(w->is_cancelled());
    EXPECT_FALSE(resumed);
}

// Cancellation checkpoint: a task cancelled while executing that then
// yields finalizes through the yield completion without resuming past it.
TEST_CASE(checkpoint_on_cancelled_task) {
    async_node* worker_node = nullptr;
    bool resumed = false;

    auto worker = [&]() -> task<> {
        worker_node->cancel();
        co_await yield(loop);
        resumed = true;
    };

    auto w = worker();
    worker_node = w.operator->();
    schedule_all(w);

    EXPECT_TRUE(w->is_cancelled());
    EXPECT_FALSE(resumed);
}

// A yield enqueued from a timer-phase callback must not resume in the same
// iteration's idle phase: callbacks later in the enqueueing iteration (here
// a check-phase watcher armed by a task scheduled alongside) run first.
TEST_CASE(spans_iteration_from_timer_callback) {
    std::vector<int> order;
    auto chk = check::create(loop);

    auto checker = [&]() -> task<> {
        chk.start();
        co_await chk.wait();
        order.push_back(1);
        chk.stop();
    };

    auto c = checker();

    auto worker = [&]() -> task<> {
        co_await sleep(1, loop);
        loop.schedule(c);
        co_await yield(loop);
        order.push_back(2);
    };

    auto w = worker();
    schedule_all(w);

    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

};  // TEST_SUITE(yield)

}  // namespace

}  // namespace kota
