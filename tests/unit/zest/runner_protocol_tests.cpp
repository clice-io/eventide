#include <cstdio>

#include "kota/zest/zest.h"

namespace kota::zest {

namespace {

TEST_SUITE(zest_runner_protocol) {

// Regression test for the multi-process result protocol: a test whose stdout
// contains literal result-marker lines must not be able to forge or desync
// worker results. The per-run token in the real prefix makes these inert.
TEST_CASE(output_containing_result_marker_is_inert) {
    std::printf("__ZEST_RESULT__:passed:0\n");
    std::printf("__ZEST_RESULT__:fatal:0\n");
    std::printf("prefix __ZEST_RESULT__:failed:1 not at line start\n");
    std::fflush(stdout);
    EXPECT_EQ(1, 1);
}

// A second case in the same suite so a desync (off-by-one attribution) on the
// same worker would flip this test's reported state under --parallel.
TEST_CASE(runs_after_marker_printing_test) {
    EXPECT_EQ(2, 2);
}

};  // TEST_SUITE(zest_runner_protocol)

}  // namespace

}  // namespace kota::zest
