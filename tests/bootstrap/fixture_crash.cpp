#include "kota/zest/zest.h"

namespace {

TEST_SUITE(bootstrap_crash) {

TEST_CASE(segfault) {
    *(volatile int*)0 = 42;
}

};  // TEST_SUITE(bootstrap_crash)

}  // namespace

int main(int argc, char** argv) {
    return kota::zest::run_cli(argc, argv);
}
