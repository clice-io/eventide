#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "kota/zest/runner/registry.h"

namespace kota::zest::detail {

constexpr inline std::string_view result_marker = "__ZEST_RESULT__:";

/// Full per-run line prefix: "__ZEST_RESULT__:<token>:". The token is
/// generated freshly for every run so test output cannot forge result lines.
std::string make_result_prefix(std::string_view token);

struct WorkerResult {
    TestState state = TestState::Fatal;
    std::chrono::milliseconds duration{0};
    std::string output;
    bool done = false;
};

std::string format_result_line(std::string_view result_prefix,
                               TestState state,
                               std::chrono::milliseconds duration);

bool parse_result_line(std::string_view result_prefix,
                       std::string_view line,
                       TestState& state,
                       std::chrono::milliseconds& duration);

void run_parallel_workers(std::string_view program,
                          const std::vector<std::string>& base_args,
                          unsigned num_workers,
                          const std::vector<std::string>& test_names,
                          std::string_view result_token,
                          std::vector<WorkerResult>& results);

}  // namespace kota::zest::detail
