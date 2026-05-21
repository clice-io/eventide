#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "kota/zest/detail/registry.h"

namespace kota::zest::detail {

inline constexpr std::string_view kResultPrefix = "__ZEST_RESULT__:";

struct WorkerResult {
    std::string test_name;
    TestState state = TestState::Fatal;
    std::chrono::milliseconds duration{0};
    std::string output;
};

std::string format_result_line(TestState state, std::chrono::milliseconds duration);

bool parse_result_line(std::string_view line, TestState& state, std::chrono::milliseconds& duration);

WorkerResult run_worker_process(const std::string& program,
                                const std::string& test_name,
                                const std::vector<std::string>& base_args);

}  // namespace kota::zest::detail
