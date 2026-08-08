#include <format>

#include "kota/zest/zest.h"

namespace kota::zest {

namespace {

int group_cases_executed = 0;

TEST_SUITE(zest_case_group) {

TEST_SUITE_ATTRS(serial = true);

TEST_CASE_GROUP(dynamic_cases) {
    for(int i = 0; i < 3; ++i) {
        add_case(std::format("case_{}", i), [] { group_cases_executed += 1; });
    }
}

// The group-level skip attr must merge with suite_attrs and keep the case
// from ever running.
TEST_CASE_GROUP(skipped_group, skip = true) {
    add_case("never_runs", [] { failure(); });
}

// Serial suites run in registration order, so the group cases above have
// already executed by the time this case checks the counter.
TEST_CASE(dynamic_cases_ran) {
    EXPECT_EQ(group_cases_executed, 3);
}

};  // TEST_SUITE(zest_case_group)

}  // namespace

}  // namespace kota::zest
