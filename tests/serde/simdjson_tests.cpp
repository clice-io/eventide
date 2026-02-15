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
#include "serde/simdjson/deserializer.h"
#include "serde/simdjson/serializer.h"

namespace serde::testing {

namespace {

using serde::json::simd::to_json;
using serde::json::simd::from_json;

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

TEST_CASE(deserialize_vector) {
    std::vector<int> value;
    auto status = from_json("[1,2,3,5,8]", value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(value, std::vector<int>({1, 2, 3, 5, 8}));
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

TEST_CASE(deserialize_map_vector) {
    std::map<std::string, std::vector<int>> value;
    auto status = from_json(R"({"a":[2,3],"b":[5]})", value);
    ASSERT_TRUE(status.has_value());
    const std::map<std::string, std::vector<int>> expected{
        {"a", {2, 3}},
        {"b", {5}   }
    };
    EXPECT_EQ(value, expected);
}

TEST_CASE(serialize_tuple) {
    std::tuple<int, char, std::string> value{42, 'x', "ok"};
    auto out = to_json(value);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([42,"x","ok"])");
}

TEST_CASE(deserialize_tuple) {
    std::tuple<int, char, std::string> value{};
    auto status = from_json(R"([42,"x","ok"])", value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(std::get<0>(value), 42);
    EXPECT_EQ(std::get<1>(value), 'x');
    EXPECT_EQ(std::get<2>(value), "ok");
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

TEST_CASE(deserialize_reflectable_struct) {
    person value{.id = 0, .name = "", .scores = {}};

    auto status = from_json(R"({"id":7,"name":"alice","scores":[10,20,30],"unknown":123})", value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(value.id, 7);
    EXPECT_EQ(value.name, "alice");
    EXPECT_EQ(value.scores, std::vector<int>({10, 20, 30}));
}

TEST_CASE(serialize_byte_span) {
    std::array<std::byte, 4> bytes{std::byte{0}, std::byte{1}, std::byte{127}, std::byte{255}};
    auto out = to_json(std::span<const std::byte>(bytes));
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([0,1,127,255])");
}

TEST_CASE(deserialize_byte_vector) {
    std::vector<std::byte> bytes;
    auto status = from_json(R"([0,1,127,255])", bytes);
    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(bytes.size(), 4U);
    EXPECT_EQ(std::to_integer<int>(bytes[0]), 0);
    EXPECT_EQ(std::to_integer<int>(bytes[1]), 1);
    EXPECT_EQ(std::to_integer<int>(bytes[2]), 127);
    EXPECT_EQ(std::to_integer<int>(bytes[3]), 255);
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

TEST_CASE(deserialize_return_value_overload) {
    auto out = from_json<std::vector<int>>(R"([7,9])");
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, std::vector<int>({7, 9}));
}

};  // TEST_SUITE(serde_simdjson)

}  // namespace

}  // namespace serde::testing
