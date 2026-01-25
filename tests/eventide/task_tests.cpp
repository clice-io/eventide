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

TEST_CASE(up_cancel, {.focus = true}) {
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
    static auto bar1 = [](int& x) -> task<int> {
        x += 1;
        co_await cancel();
        co_return 1;
    };

    static auto bar2 = [](int& x) -> task<int> {
        auto res = co_await bar1(x);
        x += 1;
        co_return res + 1;
    };

    {
        int x = 0;
        auto task = bar2(x);
        run(task);
        EXPECT_TRUE(task->is_cancelled());
        EXPECT_EQ(x, 1);
    }
}

};  // TEST_SUITE(task)

}  // namespace

}  // namespace eventide
