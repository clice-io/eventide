#include "zest/zest.h"
#include "eventide/async/loop.h"
#include "eventide/async/task.h"

namespace et = eventide;

TEST_SUITE(eventide) {

static et::task<int> foo() {
    auto frame = et::async_frame::current();
    frame->stacktrace();

    /// frame->on_cancel([&]() { std::println("foo was cancelled!"); });

    co_return 1;
}

static et::task<int> bar() {
    auto h = foo().catch_cancel();
    h->cancel();

    auto res = co_await h;
    if(!res) {
        co_return 0;
    }

    co_return 1;
}

TEST_CASE(task, {.focus = true}) {
    et::event_loop loop;
    auto task = bar();
    loop.schedule(task);
    loop.run();
    std::println("{}", task.result());
}

};  // TEST_SUITE(eventide)
