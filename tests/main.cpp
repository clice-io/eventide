#include <print>
#include <unistd.h>

#include "eventide/loop.h"
#include "eventide/stream.h"

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

int main() {
    ev::event_loop loop;
    return loop.run(echo(loop));
}
