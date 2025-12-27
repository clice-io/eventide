#include "zest/zest.h"
#include "reflection/name.h"

namespace refl::testing {

namespace {

struct struct_x;
class class_x;
enum class enum_x;
union union_x;

TEST_SUITE(reflection) {

struct struct_y;
class class_y;
enum class enum_y;
union union_y;

TEST_CASE(type_name) {
    EXPECT_EQ(type_name<struct_x>(), "struct_x");
    EXPECT_EQ(type_name<class_x>(), "class_x");
    EXPECT_EQ(type_name<enum_x>(), "enum_x");
    EXPECT_EQ(type_name<union_x>(), "union_x");

    EXPECT_EQ(type_name<struct_y>(), "struct_y");
    EXPECT_EQ(type_name<class_y>(), "class_y");
    EXPECT_EQ(type_name<enum_y>(), "enum_y");
    EXPECT_EQ(type_name<union_y>(), "union_y");

    struct struct_z;
    class class_z;
    enum class enum_z;
    union union_z;
    EXPECT_EQ(type_name<struct_z>(), "struct_z");
    EXPECT_EQ(type_name<class_z>(), "class_z");
    EXPECT_EQ(type_name<enum_z>(), "enum_z");
    EXPECT_EQ(type_name<union_z>(), "union_z");
}

struct struct_y {
    std::string x;
    std::vector<int> y_;
};

inline static struct_y ins_y;

union union_z {
    std::string x;
    std::vector<int> y_;

    union_z() {}

    ~union_z() {}
};

inline static union_z ins_y2;

TEST_CASE(field_name) {
    EXPECT_EQ(field_name<&ins_y.x>(), "x");
    EXPECT_EQ(field_name<&ins_y.y_>(), "y_");

    EXPECT_EQ(field_name<&ins_y2.x>(), "x");
    EXPECT_EQ(field_name<&ins_y2.y_>(), "y_");
}

enum class enum_y { RED, YELLOW };

TEST_CASE(enum_name) {
    EXPECT_EQ(enum_name<enum_y::RED>(), "RED");
    EXPECT_EQ(enum_name<enum_y::YELLOW>(), "YELLOW");
}

};  // TEST_SUITE(reflection)

}  // namespace

}  // namespace refl::testing
