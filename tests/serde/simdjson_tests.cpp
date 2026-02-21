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

namespace eventide::serde::testing {

namespace {

using eventide::serde::json::simd::to_json;
using eventide::serde::json::simd::from_json;

struct person {
    int id;
    std::string name;
    std::vector<int> scores;
};

struct escaped_key_person {
    int myunique = 0;
};

struct annotated_person {
    int id;
    eventide::serde::rename_alias<std::string, "displayName", "name"> name;
    eventide::serde::skip<int> internal_id;
    eventide::serde::skip_if_none<std::string> note;
};

struct point2d {
    int x;
    int y;
};

struct flattened_payload {
    int id;
    eventide::serde::flatten<point2d> point;
};

TEST_SUITE(serde_simdjson) {

TEST_CASE(serialize_vector) {
    std::vector<int> value{1, 2, 3, 5, 8};
    auto out = to_json(value);
    ASSERT_EQ(out, R"([1,2,3,5,8])");
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
    ASSERT_EQ(out, R"({"1":[2,3],"4":[5]})");
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
    ASSERT_EQ(out, R"([42,"x","ok"])");
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
    ASSERT_EQ(some_out, "42");

    std::optional<int> none = std::nullopt;
    auto none_out = to_json(none);
    ASSERT_EQ(none_out, "null");
}

TEST_CASE(serialize_variant) {
    std::variant<int, std::string> as_int = 42;
    auto int_out = to_json(as_int);
    ASSERT_EQ(int_out, "42");

    std::variant<int, std::string> as_string = std::string("ok");
    auto string_out = to_json(as_string);
    ASSERT_EQ(string_out, R"("ok")");
}

TEST_CASE(serialize_reflectable_struct) {
    person value{
        .id = 7,
        .name = "alice",
        .scores = {10, 20, 30}
    };
    auto out = to_json(value);
    ASSERT_EQ(out, R"({"id":7,"name":"alice","scores":[10,20,30]})");
}

TEST_CASE(deserialize_reflectable_struct) {
    person value{.id = 0, .name = "", .scores = {}};

    auto status = from_json(R"({"id":7,"name":"alice","scores":[10,20,30],"unknown":123})", value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(value.id, 7);
    EXPECT_EQ(value.name, "alice");
    EXPECT_EQ(value.scores, std::vector<int>({10, 20, 30}));
}

TEST_CASE(deserialize_reflectable_struct_with_nested_unknown) {
    person value{.id = 0, .name = "", .scores = {}};

    auto status = from_json(
        R"({"id":7,"unknown":{"meta":[1,2,{"k":"v"}]},"name":"alice","scores":[10,20,30]})",
        value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(value.id, 7);
    EXPECT_EQ(value.name, "alice");
    EXPECT_EQ(value.scores, std::vector<int>({10, 20, 30}));
}

TEST_CASE(deserialize_escaped_key_struct) {
    escaped_key_person value{};

    auto status = from_json(R"({"my\u0075nique":7})", value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(value.myunique, 7);
}

TEST_CASE(serialize_annotated_fields) {
    annotated_person value{};
    value.id = 7;
    static_cast<std::string&>(value.name) = "alice";
    static_cast<int&>(value.internal_id) = 100;
    static_cast<std::optional<std::string>&>(value.note) = std::nullopt;

    auto out = to_json(value);
    ASSERT_EQ(out, R"({"id":7,"displayName":"alice"})");
}

TEST_CASE(deserialize_annotated_fields) {
    annotated_person value{};
    static_cast<int&>(value.internal_id) = 41;

    auto status = from_json(R"({"id":8,"name":"bob","internal_id":100,"note":"ok"})", value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(value.id, 8);
    EXPECT_EQ(static_cast<const std::string&>(value.name), "bob");
    EXPECT_EQ(static_cast<const int&>(value.internal_id), 41);

    const auto& note = static_cast<const std::optional<std::string>&>(value.note);
    ASSERT_EQ(note, "ok");
}

TEST_CASE(serialize_flattened_field) {
    flattened_payload value{};
    value.id = 1;
    auto& point = static_cast<point2d&>(value.point);
    point.x = 2;
    point.y = 3;

    auto out = to_json(value);
    ASSERT_EQ(out, R"({"id":1,"x":2,"y":3})");
}

TEST_CASE(deserialize_flattened_field) {
    flattened_payload value{};

    auto status = from_json(R"({"id":4,"x":10,"y":20})", value);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(value.id, 4);

    const auto& point = static_cast<const point2d&>(value.point);
    EXPECT_EQ(point.x, 10);
    EXPECT_EQ(point.y, 20);
}

TEST_CASE(serialize_byte_span) {
    std::array<std::byte, 4> bytes{std::byte{0}, std::byte{1}, std::byte{127}, std::byte{255}};
    auto out = to_json(std::span<const std::byte>(bytes));
    ASSERT_EQ(out, R"([0,1,127,255])");
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
    ASSERT_EQ(out, "null");
}

TEST_CASE(serialize_with_initial_capacity) {
    std::vector<int> value{7, 9};
    auto out = to_json(value, 1);
    ASSERT_EQ(out, R"([7,9])");
}

TEST_CASE(repeat_calls_are_independent) {
    auto first = to_json(true);
    ASSERT_EQ(first, "true");

    std::vector<int> value{7, 9};
    auto second = to_json(value);
    ASSERT_EQ(second, R"([7,9])");
}

TEST_CASE(deserialize_return_value_overload) {
    auto out = from_json<std::vector<int>>(R"([7,9])");
    ASSERT_EQ(out, std::vector<int>({7, 9}));
}

};  // TEST_SUITE(serde_simdjson)

}  // namespace

}  // namespace eventide::serde::testing
