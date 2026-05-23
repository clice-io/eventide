#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "kota/zest/runner/registry.h"

namespace kota::zest::detail {

constexpr inline std::string_view result_prefix = "__ZEST_RESULT__:";
constexpr inline std::string_view frame_prefix = "__ZEST_FRAME__:";
constexpr inline std::string_view trace_begin_marker = "__ZEST_TRACE_BEGIN__";
constexpr inline std::string_view trace_end_marker = "__ZEST_TRACE_END__";

struct WorkerResult {
    std::string test_name;
    TestState state = TestState::Fatal;
    std::chrono::milliseconds duration{0};
    std::string output;
};

std::string format_result_line(TestState state, std::chrono::milliseconds duration);

bool parse_result_line(std::string_view line,
                       TestState& state,
                       std::chrono::milliseconds& duration);

#ifdef KOTA_ZEST_PARALLEL
void run_parallel_workers(std::string_view program,
                          const std::vector<std::string>& base_args,
                          unsigned num_workers,
                          const std::vector<std::string>& test_names,
                          std::vector<WorkerResult>& results);
#endif

}  // namespace kota::zest::detail
