#include <map>
#include <string>
#include <vector>

#include "serde/json.h"
#include "zest/zest.h"

namespace eventide::serde {

namespace {

struct person_simd {
    int id = 0;
    std::string name;
    std::vector<int> scores;
};

}  // namespace

template <class Serializer>
struct serialize_traits<Serializer, person_simd> {
    static void serialize(Serializer& serializer, const person_simd& value) {
        auto object = serializer.serialize_struct("person_simd", 3);
        object.serialize_field("id", value.id);
        object.serialize_field("name", value.name);
        object.serialize_field("scores", value.scores);
        object.end();
    }
};

template <>
struct deserialize_traits<eventide::serde::json::simd::Deserializer, person_simd> {
    static deserialize_result_t<person_simd, eventide::serde::json::simd::Deserializer>
        deserialize(eventide::serde::json::simd::Deserializer& deserializer,
                    value_type_t<eventide::serde::json::simd::Deserializer> value) {
        eventide::serde::json::simd::Deserializer::object_type object{};
        auto err = std::move(value.get_object()).get(object);
        if(err != simdjson::SUCCESS) {
            return std::unexpected(err);
        }

        person_simd out{};
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
            using error_t = deserialize_error_t<eventide::serde::json::simd::Deserializer>;
            return std::unexpected(detail::error_traits<error_t>::invalid_argument());
        }

        return out;
    }
};

namespace {

using serializer_t = eventide::serde::json::simd::Serializer;
using deserializer_t = eventide::serde::json::simd::Deserializer;

TEST_SUITE(serde_simdjson) {

TEST_CASE(serialize_vector) {
    std::vector<int> value{1, 2, 3, 5, 8};

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([1,2,3,5,8])");
}

TEST_CASE(deserialize_vector) {
    std::string json = R"([13,21,34])";

    deserializer_t deserializer;
    auto parsed = deserializer.parse(json);
    ASSERT_TRUE(parsed.has_value());

    auto value = eventide::serde::deserialize<std::vector<int>>(deserializer);
    ASSERT_TRUE(value.has_value());

    ASSERT_EQ(value->size(), 3U);
    EXPECT_EQ(value->at(0), 13);
    EXPECT_EQ(value->at(1), 21);
    EXPECT_EQ(value->at(2), 34);
}

TEST_CASE(serialize_map_vector) {
    std::map<int, std::vector<int>> value{
        {1, {2, 3}},
        {4, {5}   },
    };

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"1":[2,3],"4":[5]})");
}

TEST_CASE(deserialize_map_vector) {
    std::string json = R"({"1":[2,3],"4":[5]})";

    deserializer_t deserializer;
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

TEST_CASE(serialize_struct) {
    person_simd value{};
    value.id = 7;
    value.name = "alice";
    value.scores = {10, 20, 30};

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"id":7,"name":"alice","scores":[10,20,30]})");
}

TEST_CASE(deserialize_struct) {
    std::string json = R"({"id":9,"name":"bob","scores":[4,5]})";

    deserializer_t deserializer;
    auto parsed = deserializer.parse(json);
    ASSERT_TRUE(parsed.has_value());

    auto value = eventide::serde::deserialize<person_simd>(deserializer);
    ASSERT_TRUE(value.has_value());

    EXPECT_EQ(value->id, 9);
    EXPECT_EQ(value->name, "bob");
    ASSERT_EQ(value->scores.size(), 2U);
    EXPECT_EQ(value->scores.at(0), 4);
    EXPECT_EQ(value->scores.at(1), 5);
}

TEST_CASE(serialize_map_struct) {
    std::map<std::string, person_simd> value{
        {"alice", person_simd{1, "alice", {9, 8}}},
        {"bob",   person_simd{2, "bob", {7}}     },
    };

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(
        *out,
        R"({"alice":{"id":1,"name":"alice","scores":[9,8]},"bob":{"id":2,"name":"bob","scores":[7]}})");
}

TEST_CASE(deserialize_map_struct) {
    std::string json =
        R"({"alice":{"id":1,"name":"alice","scores":[9,8]},"bob":{"id":2,"name":"bob","scores":[7]}})";

    deserializer_t deserializer;
    auto parsed = deserializer.parse(json);
    ASSERT_TRUE(parsed.has_value());

    auto value = eventide::serde::deserialize<std::map<std::string, person_simd>>(deserializer);
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

TEST_CASE(parse_returns_error) {
    std::string json = R"({)";

    deserializer_t deserializer;
    auto parsed = deserializer.parse(json);

    EXPECT_FALSE(parsed.has_value());
    if(!parsed.has_value()) {
        EXPECT_NE(parsed.error(), simdjson::SUCCESS);
    }
}

};  // TEST_SUITE(serde_simdjson)

}  // namespace

}  // namespace eventide::serde
