#include "report.h"

#include <format>
#include <print>
#include <string_view>

#ifdef KOTA_ZEST_HAS_JSON
#include "kota/meta/annotation.h"
#include "kota/codec/json/json.h"
#endif

namespace kota::zest {

namespace {

constexpr std::string_view green = "\033[32m";
constexpr std::string_view yellow = "\033[33m";
constexpr std::string_view red = "\033[31m";
constexpr std::string_view clear = "\033[0m";

}  // namespace

auto state_string(TestState state) -> std::string_view {
    switch(state) {
        case TestState::Passed: return "passed";
        case TestState::Skipped: return "skipped";
        case TestState::Failed: return "failed";
        case TestState::Fatal: return "fatal";
    }
    return "unknown";
}

bool is_failure(TestState state) {
    return state == TestState::Failed || state == TestState::Fatal;
}

void print_text_result(const TestResult& result, bool verbose) {
    const bool failed = is_failure(result.state);
    if(failed && !result.captured_stdout.empty()) {
        std::println("{}", result.captured_stdout);
    }
    if(failed && !result.captured_stderr.empty()) {
        std::println(stderr, "{}", result.captured_stderr);
    }
    if(failed || verbose) {
        std::println("{0}[   {1} ] {2} ({3} ms){4}",
                     failed ? red : green,
                     failed ? "FAILED" : "    OK",
                     result.display_name,
                     result.duration.count(),
                     clear);
    }
}

void print_text_summary(const RunSummary& summary) {
    std::println("{}[----------] Global test environment tear-down. {}", green, clear);
    std::println("{}[==========] {} tests from {} test suites ran. ({} ms total){}",
                 green,
                 summary.tests,
                 summary.suites,
                 summary.duration.count(),
                 clear);

    const auto passed = summary.tests - summary.failed;
    if(passed > 0) {
        std::println("{}[  PASSED  ] {} tests.{}", green, passed, clear);
    }
    if(summary.skipped > 0) {
        std::println("{}[  SKIPPED ] {} tests.{}", yellow, summary.skipped, clear);
    }
    if(summary.failed > 0) {
        std::println("{}[  FAILED  ] {} tests, listed below:{}", red, summary.failed, clear);
        for(const auto& failed: summary.failed_tests) {
            std::println("{}[  FAILED  ] {}{}", red, failed.name, clear);
            std::println("             at {}:{}", failed.path, failed.line);
        }
        std::println("{}{} FAILED TEST{}{}",
                     red,
                     summary.failed,
                     summary.failed == 1 ? "" : "S",
                     clear);
    }
}

#ifdef KOTA_ZEST_HAS_JSON

namespace {

struct JsonTestEntry {
    std::string suite;
    kota::meta::rename<std::string, "case"> test_case;
    std::string status;
    std::uint64_t duration_ms = 0;
    kota::meta::skip_if_none<std::string> failure;
    kota::meta::annotation<std::string,
                           kota::meta::attrs::rename<"stdout">,
                           kota::meta::behavior::skip_if<kota::meta::pred::empty>>
        captured_stdout;
    kota::meta::annotation<std::string,
                           kota::meta::attrs::rename<"stderr">,
                           kota::meta::behavior::skip_if<kota::meta::pred::empty>>
        captured_stderr;
};

struct JsonTestSummary {
    std::uint64_t total = 0;
    std::uint64_t passed = 0;
    std::uint64_t failed = 0;
    std::uint64_t skipped = 0;
    std::uint64_t duration_ms = 0;
    std::vector<JsonTestEntry> tests;
};

}  // namespace

void print_json_report(const RunSummary& summary, std::span<const TestResult> results) {
    const auto passed = summary.tests - summary.failed;

    JsonTestSummary output{
        .total = passed + summary.failed + summary.skipped,
        .passed = passed,
        .failed = summary.failed,
        .skipped = summary.skipped,
        .duration_ms = static_cast<std::uint64_t>(summary.duration.count()),
        .tests = {},
    };

    output.tests.reserve(results.size());
    for(const auto& r: results) {
        auto dot = r.display_name.find('.');
        auto suite = dot != std::string::npos ? r.display_name.substr(0, dot) : r.display_name;
        auto name = dot != std::string::npos ? r.display_name.substr(dot + 1) : std::string{};

        JsonTestEntry entry{
            .suite = std::string(suite),
            .test_case = {},
            .status = std::string(state_string(r.state)),
            .duration_ms = static_cast<std::uint64_t>(r.duration.count()),
            .failure = std::nullopt,
            .captured_stdout = {},
            .captured_stderr = {},
        };
        entry.test_case = std::string(name);

        if(is_failure(r.state)) {
            entry.failure = std::format("at {}:{}", r.path, r.line);
        }
        entry.captured_stdout = r.captured_stdout;
        entry.captured_stderr = r.captured_stderr;

        output.tests.push_back(std::move(entry));
    }

    auto json = kota::codec::json::to_json(output);
    if(json.has_value()) {
        auto pretty = kota::codec::json::prettify(*json);
        std::print("{}\n", pretty.has_value() ? *pretty : *json);
    } else {
        std::println(stderr, "Error: failed to serialize JSON summary: {}", json.error().message);
    }
}

#else

void print_json_report(const RunSummary&, std::span<const TestResult>) {
    std::println(
        stderr,
        "Error: --output-format=json requires the kota::codec::json library " "(build with KOTA_CODEC_ENABLE_SIMDJSON=ON)");
}

#endif  // KOTA_ZEST_HAS_JSON

}  // namespace kota::zest
