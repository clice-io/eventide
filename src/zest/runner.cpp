#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <functional>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "capture.h"
#include "report.h"
#include "kota/deco/deco.h"
#include "kota/zest/runner/registry.h"
#include "kota/zest/runner/run.h"
#include "kota/zest/snapshot/snapshot.h"
#include "kota/support/glob_pattern.h"

#ifdef KOTA_ZEST_HAS_JSON
#include "kota/meta/annotation.h"
#include "kota/codec/json/json.h"
#endif

namespace {

using namespace kota::zest;

constexpr std::string_view wildcard_pattern = "*";
constexpr std::string_view green = "\033[32m";
constexpr std::string_view yellow = "\033[33m";
constexpr std::string_view red = "\033[31m";
constexpr std::string_view clear = "\033[0m";

struct CliOptions {
    kota::zest::Options zest;

    DecoFlag(help = "display this help and exit"; required = false; names = {"--help", "-h"})
    help = false;

    DecoInput(meta_var = "<PATTERN>"; help = "positional fallback for test name filter";
              required = false)
    <std::string> test_filter_input;
};

struct FilterPatternSet {
    kota::GlobPattern suite;
    kota::GlobPattern display;
};

auto make_display_name(std::string_view suite_name, std::string_view test_name) -> std::string {
    return std::format("{}.{}", suite_name, test_name);
}

auto resolve_filter_patterns(std::string_view filter)
    -> std::expected<FilterPatternSet, std::string> {
    if(filter.empty()) {
        return FilterPatternSet{
            .suite = *kota::GlobPattern::create("*"),
            .display = *kota::GlobPattern::create("*"),
        };
    }

    auto dot = filter.find('.');
    if(dot == std::string_view::npos) {
        auto suite_glob = kota::GlobPattern::create(filter);
        if(!suite_glob) {
            return std::unexpected(suite_glob.error().message);
        }
        auto display_glob = kota::GlobPattern::create(std::string(filter) + ".*");
        if(!display_glob) {
            return std::unexpected(display_glob.error().message);
        }
        return FilterPatternSet{
            .suite = *std::move(suite_glob),
            .display = *std::move(display_glob),
        };
    }

    auto suite_pattern = filter.substr(0, dot);
    auto test_pattern = filter.substr(dot + 1);
    if(test_pattern.empty()) {
        test_pattern = wildcard_pattern;
    }

    auto suite_glob = kota::GlobPattern::create(suite_pattern);
    if(!suite_glob) {
        return std::unexpected(suite_glob.error().message);
    }
    auto display_str = std::format("{}.{}", suite_pattern, test_pattern);
    auto display_glob = kota::GlobPattern::create(display_str);
    if(!display_glob) {
        return std::unexpected(display_glob.error().message);
    }
    return FilterPatternSet{
        .suite = *std::move(suite_glob),
        .display = *std::move(display_glob),
    };
}

using SuiteMap = std::unordered_map<std::string, std::vector<TestCase>>;

auto group_suites(const std::vector<TestSuite>& suites) -> SuiteMap {
    SuiteMap grouped_suites;
    for(const auto& suite: suites) {
        auto& target = grouped_suites[suite.name];
        auto cases = suite.cases();
        for(auto& test_case: cases) {
            target.emplace_back(std::move(test_case));
        }
    }
    return grouped_suites;
}

bool matches_suite_filter(std::string_view suite_name, const FilterPatternSet& patterns) {
    return patterns.suite.match(suite_name);
}

bool matches_test_filter(std::string_view suite_name,
                         std::string_view test_name,
                         const FilterPatternSet& patterns) {
    if(patterns.display.is_trivial_match_all()) {
        return true;
    }
    return patterns.display.match(make_display_name(suite_name, test_name));
}

bool has_focused_tests(const SuiteMap& grouped_suites, const FilterPatternSet& patterns) {
    for(const auto& [suite_name, test_cases]: grouped_suites) {
        if(!matches_suite_filter(suite_name, patterns)) {
            continue;
        }

        for(const auto& test_case: test_cases) {
            if(matches_test_filter(suite_name, test_case.name, patterns) && test_case.attrs.focus &&
               !test_case.attrs.skip) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

namespace kota::zest {

int run_cli(int argc, char** argv, std::string_view command_overview) {
    auto args = kota::deco::util::argvify(argc, argv);
    auto renderer = kota::deco::cli::text::ModernRenderer();
    kota::deco::cli::Command<CliOptions> command(command_overview);
    command.render_with(renderer);
    command.after<&CliOptions::help>([](auto& step) {
        step.print_usage();
        return step.stop();
    });

    auto parsed = command.invoke(args);
    if(!parsed.has_value()) {
        std::println(stderr, "Error parsing options: {}", parsed.error().message);
        std::exit(1);
    }

    auto& cli = parsed->options;
    if(cli.help.has_value() && *cli.help) {
        return 0;
    }

    if(cli.test_filter_input.has_value() && !cli.zest.test_filter->empty()) {
        std::println(stderr, "Error: cannot use both positional filter and --test-filter");
        std::exit(1);
    }

    if(cli.test_filter_input.has_value()) {
        cli.zest.test_filter = std::move(*cli.test_filter_input);
    }

    return run_tests(std::move(cli.zest));
}

int run_tests(Options options) {
    return Runner::instance().run_tests(std::move(options));
}

Runner& Runner::instance() {
    static Runner runner;
    return runner;
}

void Runner::add_suite(std::string_view name, std::vector<TestCase> (*cases)()) {
    suites.emplace_back(std::string(name), cases);
}

int Runner::run_tests(Options options) {
    set_update_snapshots(*options.update_snapshots);
    set_snapshot_dir(*options.snapshot_dir);

    auto patterns_result = resolve_filter_patterns(*options.test_filter);
    if(!patterns_result) {
        std::println("{}Error: invalid filter pattern: {}{}", red, patterns_result.error(), clear);
        return 1;
    }
    auto patterns = std::move(*patterns_result);
    auto grouped_suites = group_suites(suites);

    if(*options.list_tests) {
        const bool json_list = *options.output_format == OutputFormat::Json;

#ifdef KOTA_ZEST_HAS_JSON
        if(json_list) {
            struct ListEntry {
                std::string suite;
                kota::meta::rename<std::string, "case"> test_case;
            };

            std::vector<ListEntry> entries;
            for(const auto& [suite_name, test_cases]: grouped_suites) {
                if(!matches_suite_filter(suite_name, patterns)) {
                    continue;
                }
                for(const auto& test_case: test_cases) {
                    if(!matches_test_filter(suite_name, test_case.name, patterns)) {
                        continue;
                    }
                    ListEntry entry{.suite = std::string(suite_name), .test_case = {}};
                    entry.test_case = std::string(test_case.name);
                    entries.push_back(std::move(entry));
                }
            }

            auto json = kota::codec::json::to_json(entries);
            if(!json.has_value()) {
                std::println(stderr, "Error: failed to serialize test list to JSON");
                return 1;
            }
            auto pretty = kota::codec::json::prettify(*json);
            std::print("{}\n", pretty.has_value() ? *pretty : *json);
            return 0;
        }
#else
        if(json_list) {
            std::println(
                stderr,
                "Error: --output-format=json requires the kota::codec::json library " "(build with KOTA_CODEC_ENABLE_SIMDJSON=ON)");
            return 1;
        }
#endif

        for(const auto& [suite_name, test_cases]: grouped_suites) {
            if(!matches_suite_filter(suite_name, patterns)) {
                continue;
            }
            for(const auto& test_case: test_cases) {
                if(!matches_test_filter(suite_name, test_case.name, patterns)) {
                    continue;
                }
                std::println("{}", make_display_name(suite_name, test_case.name));
            }
        }
        return 0;
    }

    const bool focus_mode = has_focused_tests(grouped_suites, patterns);
    const bool verbose = *options.verbose;
    const bool json_output = *options.output_format == OutputFormat::Json;

#ifndef KOTA_ZEST_HAS_JSON
    if(json_output) {
        std::println(
            stderr,
            "Error: --output-format=json requires the kota::codec::json library " "(build with KOTA_CODEC_ENABLE_SIMDJSON=ON)");
        return 1;
    }
#endif

    RunSummary summary;

    if(!json_output) {
        std::println("{}[----------] Global test environment set-up.{}", green, clear);
        if(focus_mode) {
            std::println("{}[  FOCUS   ] Running in focus-only mode.{}", yellow, clear);
        }
    }

    struct RunnableTest {
        std::string display_name;
        std::string path;
        std::size_t line;
        bool serial;
        std::function<TestState()> test;
    };

    std::vector<RunnableTest> runnable;
    std::vector<TestResult> skipped_results;
    std::unordered_set<std::string> active_suites;

    for(auto& [suite_name, test_cases]: grouped_suites) {
        if(!matches_suite_filter(suite_name, patterns)) {
            continue;
        }

        for(auto& test_case: test_cases) {
            if(!matches_test_filter(suite_name, test_case.name, patterns)) {
                continue;
            }

            const auto display_name = make_display_name(suite_name, test_case.name);

            if(focus_mode && !test_case.attrs.focus) {
                summary.skipped += 1;
                skipped_results.push_back(TestResult{
                    .display_name = display_name,
                    .path = test_case.path,
                    .line = test_case.line,
                    .state = TestState::Skipped,
                    .duration = std::chrono::milliseconds{0},
                    .captured_stdout = {},
                    .captured_stderr = {},
                });
                continue;
            }

            if(test_case.attrs.skip) {
                if(!json_output && verbose) {
                    std::println("{}[ SKIPPED  ] {}{}", yellow, display_name, clear);
                }
                summary.skipped += 1;
                skipped_results.push_back(TestResult{
                    .display_name = display_name,
                    .path = test_case.path,
                    .line = test_case.line,
                    .state = TestState::Skipped,
                    .duration = std::chrono::milliseconds{0},
                    .captured_stdout = {},
                    .captured_stderr = {},
                });
                continue;
            }

            active_suites.insert(std::string(suite_name));
            runnable.push_back(RunnableTest{
                .display_name = display_name,
                .path = test_case.path,
                .line = test_case.line,
                .serial = test_case.attrs.serial,
                .test = std::move(test_case.test),
            });
        }
    }

    summary.suites = static_cast<std::uint32_t>(active_suites.size());
    summary.tests = static_cast<std::uint32_t>(runnable.size());

    auto run_single =
        [&](const RunnableTest& test, bool show_run_line, bool capture) -> TestResult {
        if(show_run_line && !json_output && verbose) {
            std::println("{}[ RUN      ] {}{}", green, test.display_name, clear);
        }

        using namespace std::chrono;
        std::optional<OutputCapture> guard;
        if(capture) {
            guard.emplace();
        }

        auto begin = system_clock::now();
        auto state = test.test();
        auto end = system_clock::now();

        auto captured = guard ? guard->finish() : CapturedOutput{};

        return TestResult{
            .display_name = test.display_name,
            .path = test.path,
            .line = test.line,
            .state = state,
            .duration = duration_cast<milliseconds>(end - begin),
            .captured_stdout = std::move(captured.out),
            .captured_stderr = std::move(captured.err),
        };
    };

    auto record_result = [&](const TestResult& result) {
        if(!json_output) {
            print_text_result(result, verbose);
        }
        if(is_failure(result.state)) {
            summary.failed += 1;
            summary.failed_tests.push_back(
                FailedTest{result.display_name, result.path, result.line});
        }
    };

    std::vector<TestResult> results(runnable.size());

    if(*options.parallel) {
        using namespace std::chrono;
        auto wall_begin = system_clock::now();

        std::vector<std::size_t> parallel_indices;
        std::vector<std::size_t> serial_indices;
        for(std::size_t i = 0; i < runnable.size(); ++i) {
            if(runnable[i].serial) {
                serial_indices.push_back(i);
            } else {
                parallel_indices.push_back(i);
            }
        }

        const unsigned pw = *options.parallel_workers;
        const auto num_workers = std::min(
            static_cast<std::size_t>(std::max(1u, pw ? pw : std::thread::hardware_concurrency())),
            parallel_indices.size());

        std::atomic<std::size_t> next_task{0};

        auto run_batch = [&]() {
            auto worker = [&]() {
                while(true) {
                    auto idx = next_task.fetch_add(1, std::memory_order_relaxed);
                    if(idx >= parallel_indices.size()) {
                        break;
                    }
                    auto i = parallel_indices[idx];
                    results[i] = run_single(runnable[i], false, false);
                }
            };

            {
                std::vector<std::thread> pool;
                pool.reserve(num_workers);
                for(unsigned w = 0; w < num_workers; ++w) {
                    pool.emplace_back(worker);
                }
                for(auto& t: pool) {
                    t.join();
                }
            }

            for(auto i: serial_indices) {
                results[i] = run_single(runnable[i], false, false);
            }
        };

        if(json_output) {
            OutputCapture capture;
            run_batch();
            capture.finish();
        } else {
            run_batch();
        }

        summary.duration = duration_cast<milliseconds>(system_clock::now() - wall_begin);

        for(const auto& result: results) {
            record_result(result);
        }
    } else {
        for(std::size_t i = 0; i < runnable.size(); ++i) {
            results[i] = run_single(runnable[i], true, json_output);
            record_result(results[i]);
            summary.duration += results[i].duration;
        }
    }

    results.insert(results.end(),
                   std::make_move_iterator(skipped_results.begin()),
                   std::make_move_iterator(skipped_results.end()));

    if(*options.cleanup_snapshots) {
        auto removed = cleanup_unused_snapshots();
        if(removed > 0 && !json_output) {
            std::println("[snapshot] cleaned up {} orphaned file{}",
                         removed,
                         removed == 1 ? "" : "s");
        }
    }

    if(json_output) {
        if(!print_json_report(summary, results)) {
            return 1;
        }
    } else {
        print_text_summary(summary);
    }
    return summary.failed != 0;
}

}  // namespace kota::zest
