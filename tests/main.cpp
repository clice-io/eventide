#include <print>
#include <string>
#include <string_view>
#include <unistd.h>

#include "zest/zest.h"
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

int main(int argc, char** argv) {
    std::string filter;
    constexpr std::string_view filter_prefix = "--test-filter=";
    for(int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if(arg.starts_with(filter_prefix)) {
            filter.assign(arg.substr(filter_prefix.size()));
            continue;
        }
        if(arg == "--test-filter" && i + 1 < argc) {
            filter.assign(argv[++i]);
            continue;
        }
    }

    return zest::Runner::instance().run_tests(filter);
}
