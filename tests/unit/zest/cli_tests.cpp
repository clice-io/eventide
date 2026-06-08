#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <io.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

#include "kota/zest/zest.h"

namespace kota::zest {

namespace {

#ifdef _WIN32
inline int portable_dup(int fd) {
    return _dup(fd);
}

inline int portable_dup2(int fd1, int fd2) {
    return _dup2(fd1, fd2);
}

inline int portable_close(int fd) {
    return _close(fd);
}

inline int portable_fileno(std::FILE* f) {
    return _fileno(f);
}
#else
inline int portable_dup(int fd) {
    return ::dup(fd);
}

inline int portable_dup2(int fd1, int fd2) {
    return ::dup2(fd1, fd2);
}

inline int portable_close(int fd) {
    return ::close(fd);
}

inline int portable_fileno(std::FILE* f) {
    return ::fileno(f);
}
#endif

struct CapturedOutput {
    std::string text;
    int exit_code;
};

template <typename F>
auto capture_stdout(F&& fn) -> CapturedOutput {
    std::fflush(stdout);

    int saved = portable_dup(portable_fileno(stdout));
    auto* tmp = std::tmpfile();
    portable_dup2(portable_fileno(tmp), portable_fileno(stdout));

    int code = fn();

    std::fflush(stdout);
    portable_dup2(saved, portable_fileno(stdout));
    portable_close(saved);

    std::fseek(tmp, 0, SEEK_END);
    auto size = std::ftell(tmp);
    std::fseek(tmp, 0, SEEK_SET);
    std::string buf(static_cast<std::size_t>(size), '\0');
    std::fread(buf.data(), 1, buf.size(), tmp);
    std::fclose(tmp);

    return {std::move(buf), code};
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

#ifndef _WIN32

std::string trim_left(const std::string& s) {
    auto pos = s.find_first_not_of(" \n\r\t");
    if(pos == std::string::npos) {
        return {};
    }
    return s.substr(pos);
}

CapturedOutput run_command(const std::string& cmd) {
    std::string output;
    std::array<char, 4096> buf{};
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if(!pipe) {
        return {"", -1};
    }
    while(std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        output += buf.data();
    }
    int status = ::pclose(pipe);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {std::move(output), code};
}

std::string self_exe_path() {
#ifdef __APPLE__
    std::array<char, 4096> buf{};
    std::uint32_t size = static_cast<std::uint32_t>(buf.size());
    if(::_NSGetExecutablePath(buf.data(), &size) == 0) {
        return std::string(buf.data());
    }
    return "unit_tests";
#else
    std::array<char, 4096> buf{};
    auto len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if(len <= 0) {
        return "unit_tests";
    }
    return std::string(buf.data(), static_cast<std::size_t>(len));
#endif
}

#endif  // !_WIN32

TEST_SUITE(zest_cli) {

TEST_SUITE_ATTRS(serial = true);

TEST_CASE(help_exits_zero) {
    const char* args[] = {"test", "--help"};
    auto result =
        capture_stdout([&]() { return run_cli(2, const_cast<char**>(args), "test runner"); });
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(contains(result.text, "Usage"));
}

TEST_CASE(short_help_exits_zero) {
    const char* args[] = {"test", "-h"};
    auto result =
        capture_stdout([&]() { return run_cli(2, const_cast<char**>(args), "test runner"); });
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(contains(result.text, "Usage"));
}

#ifndef _WIN32

TEST_CASE(list_tests_text) {
    auto exe = self_exe_path();
    auto result = run_command(exe + " --list-tests --test-filter=zest_check 2>/dev/null");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(contains(result.text, "zest_check."));
    EXPECT_TRUE(contains(result.text, "zest_check.binary_equal_expected_and_expected"));
    EXPECT_TRUE(contains(result.text, "zest_check.binary_ordering_expect_macros"));
}

TEST_CASE(list_tests_with_filter) {
    auto exe = self_exe_path();
    auto result =
        run_command(exe + " --list-tests '--test-filter=zest_check.binary_equal*' 2>/dev/null");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(contains(result.text, "zest_check.binary_equal_expected_and_expected"));
    EXPECT_FALSE(contains(result.text, "zest_check.binary_ordering"));
}

TEST_CASE(list_tests_json) {
    auto exe = self_exe_path();
    auto result = run_command(
        exe + " --list-tests --output-format=json --test-filter=zest_check 2>/dev/null");
    EXPECT_EQ(result.exit_code, 0);
    auto trimmed = trim_left(result.text);
    EXPECT_TRUE(!trimmed.empty() && trimmed.front() == '[');
    EXPECT_TRUE(contains(result.text, "\"suite\""));
    EXPECT_TRUE(contains(result.text, "\"case\""));
}

TEST_CASE(json_output_valid) {
    auto exe = self_exe_path();
    auto result = run_command(exe + " --output-format=json --test-filter=zest_check 2>/dev/null");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(contains(result.text, "\"total\""));
    EXPECT_TRUE(contains(result.text, "\"passed\""));
    EXPECT_TRUE(contains(result.text, "\"failed\""));
    EXPECT_TRUE(contains(result.text, "\"skipped\""));
    EXPECT_TRUE(contains(result.text, "\"tests\""));
}

TEST_CASE(json_output_no_text_pollution) {
    auto exe = self_exe_path();
    auto result = run_command(exe + " --output-format=json --test-filter=zest_check 2>/dev/null");
    EXPECT_EQ(result.exit_code, 0);
    auto trimmed = trim_left(result.text);
    EXPECT_TRUE(!trimmed.empty() && trimmed.front() == '{');
}

TEST_CASE(json_output_counts_consistent) {
    auto exe = self_exe_path();
    auto result = run_command(exe + " --output-format=json --test-filter=zest_check 2>/dev/null");
    EXPECT_EQ(result.exit_code, 0);

    auto extract_int = [&](const std::string& key) -> int {
        auto pattern = "\"" + key + "\":";
        auto pos = result.text.find(pattern);
        if(pos == std::string::npos) {
            return -1;
        }
        pos += pattern.size();
        while(pos < result.text.size() && result.text[pos] == ' ') {
            ++pos;
        }
        return std::atoi(result.text.c_str() + pos);
    };

    int cnt_total = extract_int("total");
    int cnt_passed = extract_int("passed");
    int cnt_failed = extract_int("failed");
    int cnt_skipped = extract_int("skipped");

    EXPECT_TRUE(cnt_total >= 0);
    EXPECT_TRUE(cnt_passed >= 0);
    EXPECT_TRUE(cnt_failed >= 0);
    EXPECT_TRUE(cnt_skipped >= 0);
    EXPECT_EQ(cnt_total, cnt_passed + cnt_failed + cnt_skipped);

    int test_entries = 0;
    std::string marker = "\"status\"";
    std::size_t search_pos = 0;
    while((search_pos = result.text.find(marker, search_pos)) != std::string::npos) {
        ++test_entries;
        search_pos += marker.size();
    }
    EXPECT_EQ(cnt_total, test_entries);
}

#endif  // !_WIN32

};  // TEST_SUITE(zest_cli)

}  // namespace

}  // namespace kota::zest
