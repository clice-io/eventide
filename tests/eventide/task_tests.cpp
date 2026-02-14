#include <stdexcept>

#include "zest/macro.h"
#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/task.h"

namespace eventide {

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

// MSVC's implementation of coroutines has some problems, so we cannot run the test case to pass ci.
#ifndef _MSC_VER
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

#ifdef __cpp_exceptions
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
#endif

};  // TEST_SUITE(task)

TEST_SUITE(shared_task) {

TEST_CASE(future) {
    auto task1 = []() -> shared_task<int> {
        co_return 1;
    }();

    auto task2 = [](shared_future<int> future) -> task<int> {
        auto res = co_await future;
        co_return *res + 1;
    }(task1.get());

    auto task3 = [](shared_future<int> future) -> task<int> {
        auto res = co_await future;
        co_return *res + 2;
    }(task1.get());

    auto [res2, res3, res1] = run(task2, task3, task1);
    EXPECT_EQ(res2, 2);
    EXPECT_EQ(res3, 3);
    EXPECT_EQ(res1, 1);
}

};  // TEST_SUITE(shared_task)

}  // namespace

}  // namespace eventide
