#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/reflection/compare.h"

namespace eventide::refl {

namespace {

struct c_point {
    int x;
    int y;
};

struct c_box {
    c_point pos;
    int id;
};

struct with_custom_ops {
    int x;
    int y;

    bool operator==(const with_custom_ops& other) const {
        return y == other.y;
    }

    bool operator<(const with_custom_ops& other) const {
        return y < other.y;
    }
};

struct c_point_hash {
    std::size_t operator()(const c_point& p) const {
        return (static_cast<std::size_t>(p.x) << 32) ^ static_cast<std::size_t>(p.y);
    }
};

struct c_point_equal {
    bool operator()(const c_point& lhs, const c_point& rhs) const {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }
};

TEST_SUITE(reflection) {

TEST_CASE(compare_primitive_types) {
    EXPECT_TRUE(eq(7, 7));
    EXPECT_TRUE(ne(7, 8));
    EXPECT_TRUE(lt(7, 8));
    EXPECT_TRUE(le(7, 7));
    EXPECT_TRUE(gt(9, 8));
    EXPECT_TRUE(ge(9, 9));

    EXPECT_FALSE(lt(7, 7));
    EXPECT_TRUE(le(7, 7));
    EXPECT_FALSE(gt(7, 7));
    EXPECT_TRUE(ge(7, 7));

    EXPECT_FALSE(lt(9, 7));
    EXPECT_FALSE(le(9, 7));
    EXPECT_TRUE(gt(9, 7));
    EXPECT_TRUE(ge(9, 7));
}

TEST_CASE(compare_string_like_prefers_native_operators) {
    constexpr std::string_view view = "eventide";
    constexpr char literal[] = "eventide";
    const std::string str = "eventide";

    EXPECT_TRUE(eq(view, literal));
    EXPECT_TRUE(eq(literal, view));
    EXPECT_TRUE(eq(str, literal));
    EXPECT_TRUE(eq(literal, str));
    EXPECT_FALSE(ne(view, literal));
}

TEST_CASE(compare_reflectable_struct_eq_ne) {
    c_point a{.x = 1, .y = 2};
    c_point b{.x = 1, .y = 2};
    c_point c{.x = 1, .y = 3};

    EXPECT_TRUE(eq(a, b));
    EXPECT_FALSE(ne(a, b));
    EXPECT_FALSE(eq(a, c));
    EXPECT_TRUE(ne(a, c));
}

TEST_CASE(compare_reflectable_struct_ordering) {
    c_point a{.x = 1, .y = 5};
    c_point b{.x = 2, .y = 0};
    c_point c{.x = 1, .y = 6};
    c_point same_as_a{.x = 1, .y = 5};

    EXPECT_TRUE(lt(a, b));
    EXPECT_TRUE(lt(a, c));
    EXPECT_TRUE(le(a, c));
    EXPECT_TRUE(gt(c, a));
    EXPECT_TRUE(ge(c, a));
    EXPECT_TRUE(ge(c, c));

    EXPECT_FALSE(lt(a, same_as_a));
    EXPECT_TRUE(le(a, same_as_a));
    EXPECT_FALSE(le(b, a));
    EXPECT_TRUE(gt(b, a));
    EXPECT_TRUE(ge(b, a));
}

TEST_CASE(compare_reflectable_struct_recursive) {
    c_box a{
        .pos = {.x = 1, .y = 2},
        .id = 10
    };
    c_box b{
        .pos = {.x = 1, .y = 2},
        .id = 10
    };
    c_box c{
        .pos = {.x = 1, .y = 3},
        .id = 1
    };
    c_box d{
        .pos = {.x = 2, .y = 0},
        .id = 0
    };

    EXPECT_TRUE(eq(a, b));
    EXPECT_FALSE(eq(a, c));
    EXPECT_TRUE(lt(a, c));
    EXPECT_TRUE(lt(c, d));
    EXPECT_TRUE(gt(d, c));
}

TEST_CASE(compare_sequence_ranges_of_reflectable_elements) {
    std::vector<c_point> a{
        {.x = 1, .y = 2},
        {.x = 2, .y = 3}
    };
    std::vector<c_point> b{
        {.x = 1, .y = 2},
        {.x = 2, .y = 3}
    };
    std::vector<c_point> c{
        {.x = 1, .y = 2},
        {.x = 2, .y = 4}
    };
    std::vector<c_point> d{
        {.x = 1, .y = 2},
        {.x = 2, .y = 3},
        {.x = 0, .y = 0}
    };

    EXPECT_TRUE(eq(a, b));
    EXPECT_FALSE(eq(a, c));
    EXPECT_TRUE(ne(a, c));
    EXPECT_TRUE(lt(a, c));
    EXPECT_TRUE(lt(a, d));
    EXPECT_TRUE(gt(d, a));

    EXPECT_FALSE(lt(a, b));
    EXPECT_TRUE(le(a, b));
    EXPECT_FALSE(le(c, a));
    EXPECT_TRUE(ge(c, a));
}

TEST_CASE(compare_list_of_reflectable_elements) {
    std::list<c_point> a{
        {.x = 1, .y = 2},
        {.x = 2, .y = 3}
    };
    std::list<c_point> b{
        {.x = 1, .y = 2},
        {.x = 2, .y = 3}
    };
    std::list<c_point> c{
        {.x = 1, .y = 2},
        {.x = 3, .y = 0}
    };

    EXPECT_TRUE(eq(a, b));
    EXPECT_TRUE(lt(a, c));
    EXPECT_TRUE(le(a, c));
    EXPECT_TRUE(ge(c, a));
}

TEST_CASE(compare_map_of_reflectable_values) {
    std::map<int, c_point> a{
        {1, {.x = 1, .y = 2}},
        {2, {.x = 2, .y = 3}}
    };
    std::map<int, c_point> b{
        {1, {.x = 1, .y = 2}},
        {2, {.x = 2, .y = 3}}
    };
    std::map<int, c_point> c{
        {1, {.x = 1, .y = 2}},
        {2, {.x = 2, .y = 4}}
    };
    std::map<int, c_point> d{
        {1, {.x = 1, .y = 2}},
        {3, {.x = 0, .y = 0}}
    };

    EXPECT_TRUE(eq(a, b));
    EXPECT_FALSE(eq(a, c));
    EXPECT_TRUE(ne(a, c));
    EXPECT_TRUE(lt(a, c));
    EXPECT_TRUE(lt(c, d));
    EXPECT_TRUE(gt(d, c));

    EXPECT_FALSE(lt(a, b));
    EXPECT_TRUE(le(a, b));
    EXPECT_FALSE(le(d, c));
    EXPECT_TRUE(ge(d, c));
}

TEST_CASE(compare_set_of_reflectable_values) {
    std::set<c_point, lt_fn> a{
        {.x = 1, .y = 2},
        {.x = 2, .y = 3}
    };
    std::set<c_point, lt_fn> b{
        {.x = 1, .y = 2},
        {.x = 2, .y = 3}
    };
    std::set<c_point, lt_fn> c{
        {.x = 1, .y = 2},
        {.x = 3, .y = 0}
    };

    EXPECT_TRUE(eq(a, b));
    EXPECT_FALSE(eq(a, c));
    EXPECT_TRUE(lt(a, c));
    EXPECT_TRUE(le(a, c));
    EXPECT_TRUE(ge(c, a));
    EXPECT_FALSE(lt(a, b));
    EXPECT_TRUE(le(a, b));
    EXPECT_FALSE(le(c, a));
    EXPECT_TRUE(gt(c, a));
}

TEST_CASE(compare_unordered_map_order_independent_eq) {
    std::unordered_map<int, c_point> a;
    a.emplace(1, c_point{.x = 1, .y = 2});
    a.emplace(2, c_point{.x = 2, .y = 3});

    std::unordered_map<int, c_point> b;
    b.emplace(2, c_point{.x = 2, .y = 3});
    b.emplace(1, c_point{.x = 1, .y = 2});

    std::unordered_map<int, c_point> c;
    c.emplace(2, c_point{.x = 2, .y = 4});
    c.emplace(1, c_point{.x = 1, .y = 2});

    EXPECT_TRUE(eq(a, b));
    EXPECT_FALSE(ne(a, b));
    EXPECT_FALSE(eq(a, c));
    EXPECT_TRUE(ne(a, c));
}

TEST_CASE(compare_unordered_set_order_independent_eq) {
    std::unordered_set<c_point, c_point_hash, c_point_equal> a{
        c_point{.x = 1, .y = 2},
        c_point{.x = 2, .y = 3},
    };
    std::unordered_set<c_point, c_point_hash, c_point_equal> b{
        c_point{.x = 2, .y = 3},
        c_point{.x = 1, .y = 2},
    };
    std::unordered_set<c_point, c_point_hash, c_point_equal> c{
        c_point{.x = 1, .y = 2},
        c_point{.x = 3, .y = 0},
    };

    EXPECT_TRUE(eq(a, b));
    EXPECT_FALSE(ne(a, b));
    EXPECT_FALSE(eq(a, c));
    EXPECT_TRUE(ne(a, c));
}

TEST_CASE(compare_mixed_ordered_unordered_map_eq) {
    std::map<int, c_point> ordered{
        {1, {.x = 1, .y = 2}},
        {2, {.x = 2, .y = 3}}
    };
    std::unordered_map<int, c_point> unordered{
        {2, {.x = 2, .y = 3}},
        {1, {.x = 1, .y = 2}}
    };

    EXPECT_TRUE(eq(ordered, unordered));
    EXPECT_FALSE(ne(ordered, unordered));
}

TEST_CASE(compare_uses_custom_operators_when_available) {
    with_custom_ops a{.x = 100, .y = 1};
    with_custom_ops b{.x = 0, .y = 2};
    with_custom_ops c{.x = 999, .y = 1};

    // If reflection fallback were used, x would dominate and these results would differ.
    EXPECT_TRUE(lt(a, b));
    EXPECT_TRUE(le(a, c));
    EXPECT_TRUE(eq(a, c));
    EXPECT_FALSE(ne(a, c));
}

TEST_CASE(compare_functor_for_sorting) {
    std::vector<c_point> values{
        {.x = 2, .y = 1},
        {.x = 1, .y = 4},
        {.x = 1, .y = 2},
        {.x = 1, .y = 3},
    };

    std::sort(values.begin(), values.end(), lt);

    ASSERT_EQ(values.size(), 4U);
    EXPECT_EQ(values[0].x, 1);
    EXPECT_EQ(values[0].y, 2);
    EXPECT_EQ(values[1].x, 1);
    EXPECT_EQ(values[1].y, 3);
    EXPECT_EQ(values[2].x, 1);
    EXPECT_EQ(values[2].y, 4);
    EXPECT_EQ(values[3].x, 2);
    EXPECT_EQ(values[3].y, 1);
}

};  // TEST_SUITE(reflection)

}  // namespace

}  // namespace eventide::refl
