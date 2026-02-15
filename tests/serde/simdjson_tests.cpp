#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "zest/zest.h"
#include "serde/serde.h"
#include "serde/simdjson/serializer.h"

namespace serde::testing {

namespace {

using serde::json::simd::to_json;

struct person {
    int id;
    std::string name;
    std::vector<int> scores;
};

TEST_SUITE(serde_simdjson) {

TEST_CASE(serialize_vector) {
    std::vector<int> value{1, 2, 3, 5, 8};
    auto out = to_json(value);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([1,2,3,5,8])");
}

TEST_CASE(serialize_map_vector) {
    std::map<int, std::vector<int>> value{
        {1, {2, 3}},
        {4, {5}   }
    };
    auto out = to_json(value);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"1":[2,3],"4":[5]})");
}

TEST_CASE(serialize_tuple) {
    std::tuple<int, char, std::string> value{42, 'x', "ok"};
    auto out = to_json(value);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([42,"x","ok"])");
}

TEST_CASE(serialize_optional) {
    std::optional<int> some = 42;
    auto some_out = to_json(some);
    ASSERT_TRUE(some_out.has_value());
    EXPECT_EQ(*some_out, "42");

    std::optional<int> none = std::nullopt;
    auto none_out = to_json(none);
    ASSERT_TRUE(none_out.has_value());
    EXPECT_EQ(*none_out, "null");
}

TEST_CASE(serialize_variant) {
    std::variant<int, std::string> as_int = 42;
    auto int_out = to_json(as_int);
    ASSERT_TRUE(int_out.has_value());
    EXPECT_EQ(*int_out, "42");

    std::variant<int, std::string> as_string = std::string("ok");
    auto string_out = to_json(as_string);
    ASSERT_TRUE(string_out.has_value());
    EXPECT_EQ(*string_out, R"("ok")");
}

TEST_CASE(serialize_reflectable_struct) {
    person value{
        .id = 7,
        .name = "alice",
        .scores = {10, 20, 30}
    };
    auto out = to_json(value);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"id":7,"name":"alice","scores":[10,20,30]})");
}

TEST_CASE(serialize_byte_span) {
    std::array<std::byte, 4> bytes{std::byte{0}, std::byte{1}, std::byte{127}, std::byte{255}};
    auto out = to_json(std::span<const std::byte>(bytes));
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([0,1,127,255])");
}

TEST_CASE(non_finite_float_is_null) {
    auto out = to_json(std::numeric_limits<double>::infinity());
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "null");
}

TEST_CASE(serialize_with_initial_capacity) {
    std::vector<int> value{7, 9};
    auto out = to_json(value, 1);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([7,9])");
}

TEST_CASE(repeat_calls_are_independent) {
    auto first = to_json(true);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, "true");

    std::vector<int> value{7, 9};
    auto second = to_json(value);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, R"([7,9])");
}

};  // TEST_SUITE(serde_simdjson)

}  // namespace

}  // namespace serde::testing
