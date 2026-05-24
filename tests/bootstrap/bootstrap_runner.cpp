#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <string>
#include <string_view>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif

int run_fixture(std::string_view fixture_path, std::string_view args) {
#ifdef _WIN32
    auto cmd = std::format("\"\"{}\" {}\"", fixture_path, args);
#else
    auto cmd = std::format("\"{}\" {}", fixture_path, args);
#endif

    int raw = std::system(cmd.c_str());
#ifdef _WIN32
    return raw;
#else
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
}

std::string run_fixture_output(std::string_view fixture_path, std::string_view args) {
#ifdef _WIN32
    auto cmd = std::format("\"\"{}\" {}\" 2>&1", fixture_path, args);
#else
    auto cmd = std::format("\"{}\" {} 2>&1", fixture_path, args);
#endif

    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if(pipe) {
        char buf[4096];
        while(fgets(buf, sizeof(buf), pipe)) {
            output += buf;
        }
        pclose(pipe);
    }
    return output;
}

int main(int argc, char** argv) {
    assert(argc >= 4 &&
           "usage: bootstrap_runner <fixture_path> <fixture_focus_path> <fixture_crash_path>");
    std::string fixture = argv[1];
    std::string fixture_focus = argv[2];
    std::string fixture_crash = argv[3];

    std::println("--- all tests -> non-zero exit ---");
    assert(run_fixture(fixture, "--test-filter \"*\"") != 0);

    std::println("--- passing test only -> zero exit ---");
    assert(run_fixture(fixture, "--test-filter \"bootstrap_pass.*\"") == 0);

    std::println("--- failing test only -> non-zero exit ---");
    assert(run_fixture(fixture, "--test-filter \"bootstrap_fail.*\"") != 0);

    std::println("--- skipped test only -> zero exit ---");
    assert(run_fixture(fixture, "--test-filter \"bootstrap_skip.*\"") == 0);

    std::println("--- focus mode -> zero exit ---");
    assert(run_fixture(fixture_focus, "--test-filter \"bootstrap_focus.*\"") == 0);

    std::println("--- crash test (parallel) -> non-zero exit with stack trace ---");
    {
        auto output =
            run_fixture_output(fixture_crash,
                               "--parallel --verbose --test-filter \"bootstrap_crash.*\"");
        assert(output.find("FAILED") != std::string::npos);
        assert(output.find("PASSED") != std::string::npos);
        assert(output.find("CRASH") != std::string::npos);
        assert(output.find("segfault") != std::string::npos ||
               output.find("fixture_crash") != std::string::npos);
        assert(output.find("after_crash") != std::string::npos);
    }

    std::println("all bootstrap_runner tests passed");
    return 0;
}
