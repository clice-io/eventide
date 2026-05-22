#include "kota/zest/zest.h"

namespace {

TEST_SUITE(bootstrap_focus) {

TEST_CASE(focused, focus = true) {
    EXPECT_TRUE(true);
}

TEST_CASE(unfocused) {
    EXPECT_TRUE(true);
}

};  // TEST_SUITE(bootstrap_focus)

}  // namespace

int main(int argc, char** argv) {
    return kota::zest::run_cli(argc, argv);
}
