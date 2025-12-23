#include <print>
#include <unistd.h>

#include "eventide/loop.h"
#include "eventide/stream.h"

namespace ev = eventide;

ev::task<> echo(ev::pipe& pipe) {
    while(true) {
        std::string content = co_await pipe.read();
        std::println("echo: {}", content);
    }
}

int main() {
    ev::event_loop loop;
    ev::pipe pipe(loop, STDIN_FILENO);
    return loop.run(echo(pipe));
}
