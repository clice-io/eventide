#include <print>

#include "zest/zest.h"
#include "reflection/struct.h"

struct Point {
    int x;
    int y;
};

TEST_SUITE(reflection2) {

int s = 0;

void setup() {
    s += 1;
}

TEST_CASE(struct) {
    std::println("{}", s);
};

TEST_CASE(struct2) {
    std::println("{}", s);
};

};  // TEST_SUITE(reflection2)
