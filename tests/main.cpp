#include <print>
#include <unistd.h>

#include "eventide/loop.h"
#include "eventide/stream.h"
#include "zest/zest.h"

namespace ev = eventide;

ev::task<> echo(ev::event_loop& loop) {
    auto pipe = ev::pipe::open(loop, STDIN_FILENO);
    if(!pipe) {
        co_return;
    }

    while(true) {
        std::string content = co_await pipe->read();
        std::println("echo: {}", content);
    }
}

TEST_SUITE(Test) {

TEST_CASE(A) {
    ASSERT_TRUE(false);
}

};  // TEST_SUITE(Test)

int main() {
    /// ev::event_loop loop;
    /// loop.run(echo(loop));
    return zest::Runner::instance().run_tests("");
}
