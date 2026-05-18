#pragma once

#include <string>
#include <string_view>

namespace kota::zest {

/// Runtime configuration for executing registered zest test cases.
struct RunnerOptions {
    /// Test filter in the form SUITE or SUITE.TEST, with '*' supported as a wildcard.
    std::string filter;
    /// When true, per-test output is limited to failing cases; the final summary is still printed.
    bool only_failed_output = false;
    /// When true, test cases are executed in parallel across a thread pool.
    bool parallel = false;
    /// Number of worker threads for parallel mode (0 = hardware_concurrency).
    unsigned parallel_workers = 0;
    /// When true, snapshot files are created/overwritten instead of compared.
    bool update_snapshots = false;
    /// When true, remove snapshot files that were not accessed during this run.
    bool cleanup_snapshots = false;
    /// Directory for snapshot files. If empty when a snapshot is used, a lazy error is reported.
    std::string snapshot_dir;
};

/// Parse CLI arguments into RunnerOptions and execute registered tests.
/// If default_snapshot_dir is non-empty, it is used when --snapshot-dir is not passed on the CLI.
int run_cli(int argc,
            char** argv,
            std::string_view command_overview = "unitest [options] Run unit tests",
            std::string_view default_snapshot_dir = {});

/// Execute all registered tests using an explicit runtime configuration.
int run_tests(RunnerOptions options);

/// Convenience overload for running with only a test-name filter.
int run_tests(std::string_view filter);

}  // namespace kota::zest
