#include <map>
#include <string>
#include <vector>

#include "serde/json.h"
#include "zest/zest.h"

namespace eventide::serde::testing {

namespace {

TEST_SUITE(serde_json) {

TEST_CASE(simdjson_serialize_map_vector) {
    std::map<int, std::vector<int>> value{{1, {2, 3}}, {4, {5}}};

    eventide::serde::json::simd::Serializer serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"1":[2,3],"4":[5]})");
}

TEST_CASE(simdjson_deserialize_map_vector) {
    std::string json = R"({"1":[2,3],"4":[5]})";

    eventide::serde::json::simd::Deserializer deserializer;
    auto parsed = deserializer.parse(json);
    ASSERT_TRUE(parsed.has_value());

    auto value = eventide::serde::deserialize<std::map<int, std::vector<int>>>(deserializer);
    ASSERT_TRUE(value.has_value());

    EXPECT_EQ(value->size(), 2U);
    EXPECT_EQ(value->at(1).size(), 2U);
    EXPECT_EQ(value->at(1).at(0), 2);
    EXPECT_EQ(value->at(1).at(1), 3);
    EXPECT_EQ(value->at(4).size(), 1U);
    EXPECT_EQ(value->at(4).at(0), 5);
}

TEST_CASE(simdjson_parse_returns_simd_error) {
    std::string json = R"({)";

    eventide::serde::json::simd::Deserializer deserializer;
    auto parsed = deserializer.parse(json);

    EXPECT_FALSE(parsed.has_value());
    if(!parsed.has_value()) {
        EXPECT_NE(parsed.error(), simdjson::SUCCESS);
    }
}

};  // TEST_SUITE(serde_json)

}  // namespace

}  // namespace eventide::serde::testing
