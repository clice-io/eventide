#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kota/zest/runner/registry.h"

namespace kota::zest {

struct FailedTest {
    std::string name;
    std::string path;
    std::size_t line;
};

struct RunSummary {
    std::uint32_t tests = 0;
    std::uint32_t suites = 0;
    std::uint32_t failed = 0;
    std::uint32_t skipped = 0;
    std::chrono::milliseconds duration{0};
    std::vector<FailedTest> failed_tests;
};

struct TestResult {
    std::string display_name;
    std::string path;
    std::size_t line;
    TestState state;
    std::chrono::milliseconds duration;
    std::string captured_stdout;
    std::string captured_stderr;
};

auto state_string(TestState state) -> std::string_view;
bool is_failure(TestState state);

void print_text_result(const TestResult& result, bool verbose);
void print_text_summary(const RunSummary& summary);

bool print_json_report(const RunSummary& summary, std::span<const TestResult> results);

}  // namespace kota::zest
