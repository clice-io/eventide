#pragma once

#include <string>
#include <string_view>

#include "kota/deco/deco.h"

namespace kota::zest {

/// Runtime configuration for the zest test runner.
///
/// Fields use kota::deco macros, so this struct doubles as a CLI option definition.
/// Downstream projects that need custom flags can embed it and let deco's recursive
/// parsing handle both sets transparently:
///
///     struct MyTestOptions {
///         kota::zest::Options zest;
///         DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
///                      meta_var = "<DIR>"; help = "corpus directory")
///         <std::string> corpus_dir;
///     };
///
///     // deco parses --test-filter, --snapshot-dir, AND --corpus-dir in one pass.
///     auto parsed = kota::deco::cli::parse<MyTestOptions>(args, renderer);
///     kota::zest::run_tests(std::move(parsed->options.zest));
///
struct Options {
    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate, meta_var = "<PATTERN>";
                 help = "test name filter: SUITE or SUITE.TEST or SUITE.* or *";
                 required = false)
    <std::string> test_filter = "";

    DecoFlag(help = "print all test results, not just failures"; required = false)
    verbose = false;

    DecoFlag(help = "list all registered test cases and exit"; required = false)
    list_tests = false;

    DecoFlag(help = "run test cases in parallel"; required = false)
    parallel = false;

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate, meta_var = "<N>";
                 help = "worker threads for parallel mode (0 = hardware_concurrency)";
                 required = false)
    <unsigned> parallel_workers = 0;

    DecoFlag(help = "update snapshot files instead of comparing"; required = false)
    update_snapshots = false;

    DecoFlag(help = "remove orphaned snapshot files not used in this run"; required = false)
    cleanup_snapshots = false;

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate, meta_var = "<DIR>";
                 help = "directory for snapshot files";
                 required = false)
    <std::string> snapshot_dir = "";
};

/// Parse CLI arguments and run all registered tests.
///
/// Provides a one-liner entry point for simple test executables:
///
///     int main(int argc, char** argv) {
///         return kota::zest::run_cli(argc, argv);
///     }
///
int run_cli(int argc,
            char** argv,
            std::string_view command_overview = "unitest [options] Run unit tests");

/// Run all registered tests with explicit configuration.
int run_tests(Options options);

}  // namespace kota::zest
