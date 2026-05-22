#include "kota/zest/zest.h"

namespace {

TEST_SUITE(bootstrap_pass) {

TEST_CASE(ok) {
    EXPECT_TRUE(true);
    EXPECT_EQ(1, 1);
}

};  // TEST_SUITE(bootstrap_pass)

TEST_SUITE(bootstrap_fail) {

TEST_CASE(mismatch) {
    EXPECT_EQ(1, 2);
}

};  // TEST_SUITE(bootstrap_fail)

TEST_SUITE(bootstrap_skip) {

TEST_CASE(skipped, skip = true) {
    EXPECT_TRUE(false);
}

};  // TEST_SUITE(bootstrap_skip)

}  // namespace

int main(int argc, char** argv) {
    return kota::zest::run_cli(argc, argv);
}
