#include <format>

#include "kota/zest/zest.h"
#include "kota/support/string_ref.h"

namespace kota {
namespace {

TEST_SUITE(string_ref) {

TEST_CASE(std_format) {
    EXPECT_EQ(std::format("{}", string_ref("abc")), "abc");
}

};  // TEST_SUITE(string_ref)

}  // namespace
}  // namespace kota
