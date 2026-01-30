#include <atomic>

#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/task.h"
#include "eventide/when.h"

namespace eventide {

namespace {

TEST_SUITE(when_ops) {

TEST_CASE(when_all_values) {
    auto a = []() -> task<int> {
        co_return 1;
    };

    auto b = []() -> task<int> {
        co_return 2;
    };

    auto combined = [&]() -> task<int> {
        auto [x, y] = co_await when_all(a(), b());
        co_return x + y;
    };

    auto [res] = run(combined());
    EXPECT_EQ(res, 3);
}

TEST_CASE(when_any_first_wins) {
    std::atomic<int> a_count{0};
    std::atomic<int> b_count{0};

    auto a = [&]() -> task<int> {
        a_count.fetch_add(1);
        co_return 10;
    };

    auto b = [&]() -> task<int> {
        b_count.fetch_add(1);
        co_return 20;
    };

    auto combined = [&]() -> task<std::size_t> {
        auto idx = co_await when_any(a(), b());
        co_return idx;
    };

    auto [idx] = run(combined());
    EXPECT_EQ(idx, 0U);
    EXPECT_EQ(a_count.load(), 1);
    EXPECT_EQ(b_count.load(), 0);
}

};  // TEST_SUITE(when_ops)

}  // namespace

}  // namespace eventide
