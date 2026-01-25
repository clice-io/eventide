#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/task.h"

namespace eventide {

namespace {

TEST_SUITE(task) {

task<int> foo() {
    co_return 1;
}

task<int> foo1() {
    co_return co_await foo() + 1;
}

task<int> foo2() {
    auto res = co_await foo();
    auto res1 = co_await foo1();
    co_return res + res1;
}

TEST_CASE(await) {
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

TEST_CASE(cancel) {}

};  // TEST_SUITE(task)

}  // namespace

}  // namespace eventide
