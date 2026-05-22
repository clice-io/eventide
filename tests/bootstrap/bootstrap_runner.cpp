#include <cassert>
#include <cstdio>
#include <print>
#include <string>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

struct RunResult {
    int exit_code;
    std::string output;
};

RunResult run_fixture(const std::string& fixture_path, const std::string& args) {
    std::string cmd = "\"" + fixture_path + "\"" + " " + args + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    assert(pipe && "popen failed");

    std::string output;
    char buf[256];
    while(std::fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }

    int raw = pclose(pipe);
    int exit_code;
#ifdef _WIN32
    exit_code = raw;
#else
    if(WIFEXITED(raw)) {
        exit_code = WEXITSTATUS(raw);
    } else {
        exit_code = -1;
    }
#endif

    return {exit_code, std::move(output)};
}

int main(int argc, char** argv) {
    assert(argc >= 2 && "usage: bootstrap_runner <fixture_path>");
    std::string fixture = argv[1];

    // 1. All tests -> non-zero exit (because bootstrap_fail.mismatch fails)
    std::println("--- all tests -> non-zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"*\"");
        assert(result.exit_code != 0);
    }

    // 2. Passing test only -> zero exit
    std::println("--- passing test only -> zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"bootstrap_pass.*\"");
        assert(result.exit_code == 0);
    }

    // 3. Failing test only -> non-zero exit
    std::println("--- failing test only -> non-zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"bootstrap_fail.*\"");
        assert(result.exit_code != 0);
    }

    // 4. Output contains expected markers
    std::println("--- output contains expected markers ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"*\"");
        assert(result.output.find("passed") != std::string::npos ||
               result.output.find("Passed") != std::string::npos ||
               result.output.find("PASSED") != std::string::npos);
        assert(result.output.find("failed") != std::string::npos ||
               result.output.find("Failed") != std::string::npos ||
               result.output.find("FAILED") != std::string::npos);
        assert(result.output.find("skipped") != std::string::npos ||
               result.output.find("Skipped") != std::string::npos ||
               result.output.find("SKIPPED") != std::string::npos);
    }

    // 5. Skipped test only -> zero exit
    std::println("--- skipped test only -> zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"bootstrap_skip.*\"");
        assert(result.exit_code == 0);
    }

    std::println("all bootstrap_runner tests passed");
    return 0;
}
