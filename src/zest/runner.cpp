#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <expected>
#include <filesystem>
#include <functional>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "kota/deco/deco.h"
#include "kota/deco/detail/text.h"
#include "kota/zest/detail/registry.h"
#include "kota/zest/detail/snapshot.h"
#include "kota/zest/run.h"
#include "kota/support/glob_pattern.h"
#include "worker.h"

namespace {

constexpr std::string_view wildcard_pattern = "*";
constexpr std::string_view green = "\033[32m";
constexpr std::string_view yellow = "\033[33m";
constexpr std::string_view red = "\033[31m";
constexpr std::string_view clear = "\033[0m";

struct CliOptions {
    kota::zest::Options zest;

    DecoInput(meta_var = "<PATTERN>"; help = "positional fallback for test name filter";
              required = false)
    <std::string> test_filter_input;
};

struct FilterPatternSet {
    kota::GlobPattern suite;
    kota::GlobPattern display;
};

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
    kota::zest::TestState state;
    std::chrono::milliseconds duration;
    std::string output;
};

using SuiteMap = std::unordered_map<std::string, std::vector<kota::zest::TestCase>>;

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

auto group_suites(const std::vector<kota::zest::TestSuite>& suites) -> SuiteMap {
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

bool is_failure(kota::zest::TestState state) {
    return state == kota::zest::TestState::Failed || state == kota::zest::TestState::Fatal;
}

void print_run_result(std::string_view display_name,
                      bool failed,
                      std::chrono::milliseconds duration,
                      bool only_failed_output) {
    if(failed || !only_failed_output) {
        std::println("{0}[   {1} ] {2} ({3} ms){4}",
                     failed ? red : green,
                     failed ? "FAILED" : "    OK",
                     display_name,
                     duration.count(),
                     clear);
    }
}

void print_summary(const RunSummary& summary) {
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

std::string resolve_program_path(const char* argv0) {
#if defined(__linux__)
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if(!ec) {
        return p.string();
    }
#endif
    std::error_code ec2;
    auto p2 = std::filesystem::canonical(argv0, ec2);
    if(!ec2) {
        return p2.string();
    }
    return argv0;
}

}  // namespace

namespace kota::zest {

int run_cli(int argc, char** argv, std::string_view command_overview) {
    // Intercept --zest-worker before deco parsing.
    bool worker_mode = false;
    std::vector<const char*> clean_argv;
    for(int i = 0; i < argc; ++i) {
        if(std::strcmp(argv[i], "--zest-worker") == 0) {
            worker_mode = true;
        } else {
            clean_argv.push_back(argv[i]);
        }
    }

    auto args = kota::deco::util::argvify(
        static_cast<int>(clean_argv.size()),
        const_cast<char**>(clean_argv.data()));
    auto renderer = kota::deco::cli::text::ModernRenderer();
    kota::deco::cli::Command<CliOptions> command(command_overview);
    command.render_with(renderer);

    auto parsed = kota::deco::cli::parse<CliOptions>(args, renderer);
    if(!parsed.has_value()) {
        if(worker_mode) {
            std::println("{}", detail::format_result_line(TestState::Fatal, std::chrono::milliseconds{0}));
            return 1;
        }
        std::cerr << "Error parsing options: " << parsed.error().message << "\n";
        command.usage(std::cerr);
        std::exit(1);
    }

    auto& cli = parsed->options;
    if(cli.test_filter_input.has_value() && !cli.zest.test_filter->empty()) {
        std::cerr << "Error: cannot use both positional filter and --test-filter\n";
        command.usage(std::cerr);
        std::exit(1);
    }

    if(cli.test_filter_input.has_value()) {
        cli.zest.test_filter = std::move(*cli.test_filter_input);
    }

    if(worker_mode) {
        return Runner::instance().run_as_worker(std::move(cli.zest));
    }

    cli.zest._program = resolve_program_path(argv[0]);
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

int Runner::run_as_worker(Options options) {
    set_update_snapshots(*options.update_snapshots);
    set_snapshot_dir(*options.snapshot_dir);

    auto grouped = group_suites(suites);
    auto patterns = resolve_filter_patterns(*options.test_filter);
    if(!patterns) {
        std::println("{}", detail::format_result_line(TestState::Fatal, std::chrono::milliseconds{0}));
        return 1;
    }

    for(auto& [suite_name, test_cases]: grouped) {
        if(!matches_suite_filter(suite_name, *patterns)) {
            continue;
        }
        for(auto& tc: test_cases) {
            if(!matches_test_filter(suite_name, tc.name, *patterns)) {
                continue;
            }
            if(tc.attrs.skip) {
                std::println("{}", detail::format_result_line(TestState::Skipped, std::chrono::milliseconds{0}));
                return 0;
            }

            using namespace std::chrono;
            auto begin = system_clock::now();
            auto state = tc.test();
            auto end = system_clock::now();
            auto dur = duration_cast<milliseconds>(end - begin);

            std::println("{}", detail::format_result_line(state, dur));
            return 0;
        }
    }

    std::println("{}", detail::format_result_line(TestState::Fatal, std::chrono::milliseconds{0}));
    return 1;
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
    const bool focus_mode = has_focused_tests(grouped_suites, patterns);

    const bool only_failed_output = *options.only_failed;

    RunSummary summary;

    std::println("{}[----------] Global test environment set-up.{}", green, clear);
    if(focus_mode) {
        std::println("{}[  FOCUS   ] Running in focus-only mode.{}", yellow, clear);
    }

    // Collect all runnable test cases.
    struct RunnableTest {
        std::string display_name;
        std::string path;
        std::size_t line;
        bool serial;
        std::function<TestState()> test;
    };

    std::vector<RunnableTest> runnable;
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
                continue;
            }

            if(test_case.attrs.skip) {
                if(!only_failed_output) {
                    std::println("{}[ SKIPPED  ] {}{}", yellow, display_name, clear);
                }
                summary.skipped += 1;
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

    auto run_single = [&](const RunnableTest& test, bool show_run_line) -> TestResult {
        if(show_run_line && !only_failed_output) {
            std::println("{}[ RUN      ] {}{}", green, test.display_name, clear);
        }

        using namespace std::chrono;
        auto begin = system_clock::now();
        auto state = test.test();
        auto end = system_clock::now();

        return TestResult{
            .display_name = test.display_name,
            .path = test.path,
            .line = test.line,
            .state = state,
            .duration = duration_cast<milliseconds>(end - begin),
            .output = {},
        };
    };

    auto record_result = [&](const TestResult& result) {
        const bool failed = is_failure(result.state);
        if(failed && !result.output.empty()) {
            std::println("{}", result.output);
        }
        print_run_result(result.display_name, failed, result.duration, only_failed_output);
        summary.duration += result.duration;
        if(failed) {
            summary.failed += 1;
            summary.failed_tests.push_back(
                FailedTest{result.display_name, result.path, result.line});
        }
    };

    // Execute tests.
    std::vector<TestResult> results(runnable.size());

    if(*options.parallel) {
        using namespace std::chrono;
        auto wall_begin = system_clock::now();

        // Build base args forwarded to each worker process.
        std::vector<std::string> base_args;
        if(!options.snapshot_dir->empty()) {
            base_args.push_back("--snapshot-dir=" + *options.snapshot_dir);
        }
        if(*options.update_snapshots) {
            base_args.push_back("--update-snapshots");
        }

        // Partition: parallel-safe tests first, serial tests after.
        std::vector<std::size_t> parallel_indices;
        std::vector<std::size_t> serial_indices;
        for(std::size_t i = 0; i < runnable.size(); ++i) {
            if(runnable[i].serial) {
                serial_indices.push_back(i);
            } else {
                parallel_indices.push_back(i);
            }
        }

        // Run parallel-safe tests in separate worker processes.
        const unsigned pw = *options.parallel_workers;
        const auto num_workers = std::min(
            static_cast<std::size_t>(std::max(1u, pw ? pw : std::thread::hardware_concurrency())),
            parallel_indices.size());

        std::atomic<std::size_t> next_task{0};

        auto coordinator = [&]() {
            while(true) {
                auto idx = next_task.fetch_add(1, std::memory_order_relaxed);
                if(idx >= parallel_indices.size()) {
                    break;
                }
                auto i = parallel_indices[idx];
                auto wr = detail::run_worker_process(
                    options._program, runnable[i].display_name, base_args);

                results[i] = TestResult{
                    .display_name = runnable[i].display_name,
                    .path = runnable[i].path,
                    .line = runnable[i].line,
                    .state = wr.state,
                    .duration = wr.duration,
                    .output = std::move(wr.output),
                };
            }
        };

        {
            std::vector<std::thread> pool;
            pool.reserve(num_workers);
            for(std::size_t w = 0; w < num_workers; ++w) {
                pool.emplace_back(coordinator);
            }
            for(auto& t: pool) {
                t.join();
            }
        }

        // Run serial tests sequentially in-process after the parallel batch.
        for(auto i: serial_indices) {
            results[i] = run_single(runnable[i], false);
        }

        summary.duration = duration_cast<milliseconds>(system_clock::now() - wall_begin);

        // Print all results in original order.
        for(const auto& result: results) {
            const bool failed = is_failure(result.state);
            if(failed && !result.output.empty()) {
                std::println("{}", result.output);
            }
            print_run_result(result.display_name, failed, result.duration, only_failed_output);
            if(failed) {
                summary.failed += 1;
                summary.failed_tests.push_back(
                    FailedTest{result.display_name, result.path, result.line});
            }
        }
    } else {
        for(std::size_t i = 0; i < runnable.size(); ++i) {
            results[i] = run_single(runnable[i], true);
            record_result(results[i]);
        }
    }

    if(*options.cleanup_snapshots) {
        auto removed = cleanup_unused_snapshots();
        if(removed > 0) {
            std::println("[snapshot] cleaned up {} orphaned file{}",
                         removed,
                         removed == 1 ? "" : "s");
        }
    }

    print_summary(summary);
    return summary.failed != 0;
}

}  // namespace kota::zest
