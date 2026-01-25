#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/task.h"

namespace et = eventide;

TEST_SUITE(eventide) {

static et::task<int> foo() {
    auto frame = et::async_node::current();
    ///frame->stacktrace();

    /// frame->on_cancel([&]() { std::println("foo was cancelled!"); });

    co_return 1;
}

static et::task<int> bar() {
    auto h = foo().catch_cancel();
    h->cancel();

    auto res = co_await std::move(h);
    if(!res) {
        co_return 0;
    }

    co_return 1;
}

TEST_CASE(task, {.focus = true}) {
    et::event_loop loop;
    auto task = bar();
    loop.schedule(std::move(task));
    loop.run();
    std::println("{}", task.result());
}

};  // TEST_SUITE(eventide)
