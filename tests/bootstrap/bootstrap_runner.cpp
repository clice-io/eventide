#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

struct RunResult {
    int exit_code;
    std::string output;
};

RunResult run_fixture(const std::string& fixture_path, const std::string& args) {
    auto tmp = std::filesystem::temp_directory_path() / "zest_bootstrap_output.txt";
    auto tmp_str = tmp.string();

#ifdef _WIN32
    std::string cmd = "\"\"" + fixture_path + "\" " + args + " > \"" + tmp_str + "\" 2>&1\"";
#else
    std::string cmd = "\"" + fixture_path + "\" " + args + " > \"" + tmp_str + "\" 2>&1";
#endif

    int raw = std::system(cmd.c_str());
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

    std::string output;
    if(std::ifstream ifs(tmp_str); ifs) {
        std::ostringstream ss;
        ss << ifs.rdbuf();
        output = ss.str();
    }

    std::filesystem::remove(tmp);
    return {exit_code, std::move(output)};
}

int main(int argc, char** argv) {
    assert(argc >= 3 && "usage: bootstrap_runner <fixture_path> <fixture_focus_path>");
    std::string fixture = argv[1];
    std::string fixture_focus = argv[2];

    std::println("--- all tests -> non-zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"*\"");
        assert(result.exit_code != 0);
    }

    std::println("--- passing test only -> zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"bootstrap_pass.*\"");
        assert(result.exit_code == 0);
    }

    std::println("--- failing test only -> non-zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"bootstrap_fail.*\"");
        assert(result.exit_code != 0);
    }

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

    std::println("--- skipped test only -> zero exit ---");
    {
        auto result = run_fixture(fixture, "--test-filter \"bootstrap_skip.*\"");
        assert(result.exit_code == 0);
    }

    std::println("--- focus mode ---");
    {
        auto result = run_fixture(fixture_focus, "--test-filter \"bootstrap_focus.*\"");
        assert(result.exit_code == 0);
        assert(result.output.find("SKIPPED") != std::string::npos ||
               result.output.find("skipped") != std::string::npos);
    }

    std::println("all bootstrap_runner tests passed");
    return 0;
}
