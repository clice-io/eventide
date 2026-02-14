#include <cstdint>

#include "zest/zest.h"
#include "reflection/enum.h"

namespace refl::testing {

namespace {

enum class Color {
    Red,
    Green,
    Blue,
};

enum Plain {
    Zero,
    One,
    Two,
};

enum class Tiny : std::uint8_t {
    A,
    B,
    C,
};

static_assert(refl::reflection<Color>::member_count == 3);
static_assert(refl::reflection<Color>::member_names.size() == 3);
static_assert(refl::reflection<Color>::member_names[0] == "Red");
static_assert(refl::reflection<Color>::member_names[1] == "Green");
static_assert(refl::reflection<Color>::member_names[2] == "Blue");

static_assert(refl::reflection<Plain>::member_count == 3);
static_assert(refl::reflection<Plain>::member_names.size() == 3);
static_assert(refl::reflection<Plain>::member_names[0] == "Zero");
static_assert(refl::reflection<Plain>::member_names[1] == "One");
static_assert(refl::reflection<Plain>::member_names[2] == "Two");

static_assert(refl::reflection<Tiny>::member_count == 3);
static_assert(refl::reflection<Tiny>::member_names.size() == 3);
static_assert(refl::reflection<Tiny>::member_names[0] == "A");
static_assert(refl::reflection<Tiny>::member_names[1] == "B");
static_assert(refl::reflection<Tiny>::member_names[2] == "C");

TEST_SUITE(reflection) {

TEST_CASE(enum_member_names) {
    EXPECT_EQ(refl::enum_name(Color::Red), "Red");
    EXPECT_EQ(refl::enum_name(Color::Green), "Green");
    EXPECT_EQ(refl::enum_name(Color::Blue), "Blue");

    EXPECT_EQ(refl::enum_name(Plain::Zero), "Zero");
    EXPECT_EQ(refl::enum_name(Plain::One), "One");
    EXPECT_EQ(refl::enum_name(Plain::Two), "Two");

    EXPECT_EQ(refl::enum_name(Tiny::A), "A");
    EXPECT_EQ(refl::enum_name(Tiny::B), "B");
    EXPECT_EQ(refl::enum_name(Tiny::C), "C");
}

};  // TEST_SUITE(reflection)

}  // namespace

}  // namespace refl::testing
