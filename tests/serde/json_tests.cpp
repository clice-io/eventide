#include <map>
#include <string>
#include <vector>

#include "zest/zest.h"
#include "serde/json.h"

namespace eventide::serde::testing {

struct person {
    int id = 0;
    std::string name;
    std::vector<int> scores;
};

}  // namespace eventide::serde::testing

namespace eventide::serde {

template <>
struct serialize_traits<eventide::serde::testing::person> {
    template <class Serializer>
    static void serialize(const eventide::serde::testing::person& value, Serializer& serializer) {
        auto object = serializer.serialize_struct("person", 3);
        object.serialize_field("id", value.id);
        object.serialize_field("name", value.name);
        object.serialize_field("scores", value.scores);
        object.end();
    }
};

template <>
struct deserialize_traits<eventide::serde::testing::person> {
    template <class Deserializer>
    static deserialize_result_t<eventide::serde::testing::person, Deserializer>
        deserialize(Deserializer& deserializer, value_type_t<Deserializer> value) {
        using object_t = typename Deserializer::object_type;
        object_t object{};
        auto err = std::move(value.get_object()).get(object);
        if(err != simdjson::SUCCESS) {
            return std::unexpected(err);
        }

        eventide::serde::testing::person out{};
        bool has_id = false;
        bool has_name = false;
        bool has_scores = false;

        for(auto field: object) {
            if(field.key == "id") {
                auto parsed = eventide::serde::deserialize<int>(deserializer, field.value);
                if(!parsed) {
                    return std::unexpected(parsed.error());
                }
                out.id = *parsed;
                has_id = true;
            } else if(field.key == "name") {
                auto parsed = eventide::serde::deserialize<std::string>(deserializer, field.value);
                if(!parsed) {
                    return std::unexpected(parsed.error());
                }
                out.name = std::move(*parsed);
                has_name = true;
            } else if(field.key == "scores") {
                auto parsed =
                    eventide::serde::deserialize<std::vector<int>>(deserializer, field.value);
                if(!parsed) {
                    return std::unexpected(parsed.error());
                }
                out.scores = std::move(*parsed);
                has_scores = true;
            }
        }

        if(!has_id || !has_name || !has_scores) {
            using error_t = deserialize_error_t<Deserializer>;
            return std::unexpected(detail::error_traits<error_t>::invalid_argument());
        }

        return out;
    }
};

}  // namespace eventide::serde

namespace eventide::serde::testing {

namespace {

TEST_SUITE(serde_json) {

TEST_CASE(simdjson_serialize_vector) {
    std::vector<int> value{1, 2, 3, 5, 8};

    eventide::serde::json::simd::Serializer serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([1,2,3,5,8])");
}

TEST_CASE(simdjson_deserialize_vector) {
    std::string json = R"([13,21,34])";

    eventide::serde::json::simd::Deserializer deserializer;
    auto parsed = deserializer.parse(json);
    ASSERT_TRUE(parsed.has_value());

    auto value = eventide::serde::deserialize<std::vector<int>>(deserializer);
    ASSERT_TRUE(value.has_value());

    ASSERT_EQ(value->size(), 3U);
    EXPECT_EQ(value->at(0), 13);
    EXPECT_EQ(value->at(1), 21);
    EXPECT_EQ(value->at(2), 34);
}

TEST_CASE(simdjson_serialize_map_vector) {
    std::map<int, std::vector<int>> value{
        {1, {2, 3}},
        {4, {5}   }
    };

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

TEST_CASE(simdjson_serialize_struct) {
    person value{};
    value.id = 7;
    value.name = "alice";
    value.scores = {10, 20, 30};

    eventide::serde::json::simd::Serializer serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"id":7,"name":"alice","scores":[10,20,30]})");
}

TEST_CASE(simdjson_deserialize_struct) {
    std::string json = R"({"id":9,"name":"bob","scores":[4,5]})";

    eventide::serde::json::simd::Deserializer deserializer;
    auto parsed = deserializer.parse(json);
    ASSERT_TRUE(parsed.has_value());

    auto value = eventide::serde::deserialize<person>(deserializer);
    ASSERT_TRUE(value.has_value());

    EXPECT_EQ(value->id, 9);
    EXPECT_EQ(value->name, "bob");
    ASSERT_EQ(value->scores.size(), 2U);
    EXPECT_EQ(value->scores.at(0), 4);
    EXPECT_EQ(value->scores.at(1), 5);
}

TEST_CASE(simdjson_serialize_map_struct) {
    std::map<std::string, person> value;
    value["alice"] = person{
        1,
        "alice",
        {9, 8}
    };
    value["bob"] = person{2, "bob", {7}};

    eventide::serde::json::simd::Serializer serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(
        *out,
        R"({"alice":{"id":1,"name":"alice","scores":[9,8]},"bob":{"id":2,"name":"bob","scores":[7]}})");
}

TEST_CASE(simdjson_deserialize_map_struct) {
    std::string json =
        R"({"alice":{"id":1,"name":"alice","scores":[9,8]},"bob":{"id":2,"name":"bob","scores":[7]}})";

    eventide::serde::json::simd::Deserializer deserializer;
    auto parsed = deserializer.parse(json);
    ASSERT_TRUE(parsed.has_value());

    auto value = eventide::serde::deserialize<std::map<std::string, person>>(deserializer);
    ASSERT_TRUE(value.has_value());

    ASSERT_EQ(value->size(), 2U);
    ASSERT_TRUE(value->contains("alice"));
    ASSERT_TRUE(value->contains("bob"));

    EXPECT_EQ(value->at("alice").id, 1);
    EXPECT_EQ(value->at("alice").name, "alice");
    ASSERT_EQ(value->at("alice").scores.size(), 2U);
    EXPECT_EQ(value->at("alice").scores.at(0), 9);
    EXPECT_EQ(value->at("alice").scores.at(1), 8);

    EXPECT_EQ(value->at("bob").id, 2);
    EXPECT_EQ(value->at("bob").name, "bob");
    ASSERT_EQ(value->at("bob").scores.size(), 1U);
    EXPECT_EQ(value->at("bob").scores.at(0), 7);
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
