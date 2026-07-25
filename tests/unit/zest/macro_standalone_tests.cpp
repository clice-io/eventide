// "kota/zest/macro.h" is what a downstream consuming kotatsu as a module has to
// include: modules cannot export macros, so the test macros must arrive through
// a textual include that drags in no zest declarations of its own. Including it
// as the very first header here keeps that contract honest — if it ever grows a
// dependency on another zest header, this translation unit stops compiling.
#include "kota/zest/macro.h"

#if !defined(TEST_SUITE) || !defined(TEST_CASE) || !defined(EXPECT_TRUE) || !defined(EXPECT_EQ) || \
    !defined(ASSERT_TRUE) || !defined(EXPECT_SNAPSHOT) || !defined(STATIC_EXPECT_EQ) ||            \
    !defined(EXPECT_SNAPSHOT_JSON)
#error "kota/zest/macro.h must define the zest test macros on its own"
#endif

#include "kota/zest/zest.h"

namespace kota::zest {

namespace {

// Written against the macros already in scope from the standalone include above,
// which is the order a module consumer ends up with.
TEST_SUITE(zest_macro_standalone) {

TEST_CASE(macros_usable_without_declaration_headers) {
    STATIC_EXPECT_EQ(1 + 1, 2);
    ASSERT_TRUE(true);
    EXPECT_EQ(std::string("a"), std::string("a"));
}

};  // TEST_SUITE(zest_macro_standalone)

}  // namespace

}  // namespace kota::zest
