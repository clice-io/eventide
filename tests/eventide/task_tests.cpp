#include "zest/zest.h"
#include "eventide/async/task.h"

namespace et = eventide;

TEST_SUITE(eventide) {

static et::task<int> foo() {
    co_return 1;
}

TEST_CASE(task, {.focus = true}) {}

};  // TEST_SUITE(eventide)
