#undef NDEBUG
#include <cassert>
#include <cstdlib>
#include <print>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

int run_fixture(const std::string& fixture_path, const std::string& args) {
#ifdef _WIN32
    std::string cmd = "\"\"" + fixture_path + "\" " + args + "\"";
#else
    std::string cmd = "\"" + fixture_path + "\" " + args;
#endif

    int raw = std::system(cmd.c_str());
#ifdef _WIN32
    return raw;
#else
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
}

int main(int argc, char** argv) {
    assert(argc >= 3 && "usage: bootstrap_runner <fixture_path> <fixture_focus_path>");
    std::string fixture = argv[1];
    std::string fixture_focus = argv[2];

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

    std::println("all bootstrap_runner tests passed");
    return 0;
}
