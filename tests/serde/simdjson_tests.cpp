#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include "zest/zest.h"
#include "serde/serde.h"
#include "serde/simdjson/serializer.h"

namespace serde::testing {

namespace {

using serializer_t = serde::json::simd::Serializer;

struct person {
    int id;
    std::string name;
    std::vector<int> scores;
};

TEST_SUITE(serde_simdjson) {

TEST_CASE(serialize_vector) {
    std::vector<int> value{1, 2, 3, 5, 8};

    serializer_t serializer;
    auto result = serde::serialize(serializer, value);
    ASSERT_TRUE(result.has_value());

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([1,2,3,5,8])");
}

TEST_CASE(serialize_map_vector) {
    std::map<int, std::vector<int>> value{
        {1, {2, 3}},
        {4, {5}   }
    };

    serializer_t serializer;
    auto result = serde::serialize(serializer, value);
    ASSERT_TRUE(result.has_value());

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"1":[2,3],"4":[5]})");
}

TEST_CASE(serialize_tuple) {
    std::tuple<int, char, std::string> value{42, 'x', "ok"};

    serializer_t serializer;
    auto result = serde::serialize(serializer, value);
    ASSERT_TRUE(result.has_value());

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([42,"x","ok"])");
}

TEST_CASE(serialize_reflectable_struct) {
    person value{
        .id = 7,
        .name = "alice",
        .scores = {10, 20, 30}
    };

    serializer_t serializer;
    auto result = serde::serialize(serializer, value);
    ASSERT_TRUE(result.has_value());

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"id":7,"name":"alice","scores":[10,20,30]})");
}

TEST_CASE(serialize_byte_span) {
    std::array<std::byte, 4> bytes{std::byte{0}, std::byte{1}, std::byte{127}, std::byte{255}};

    serializer_t serializer;
    auto result = serde::serialize(serializer, std::span<const std::byte>(bytes));
    ASSERT_TRUE(result.has_value());

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([0,1,127,255])");
}

TEST_CASE(non_finite_float_is_null) {
    serializer_t serializer;
    auto result = serializer.serialize_float(std::numeric_limits<double>::infinity());
    ASSERT_TRUE(result.has_value());

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "null");
}

TEST_CASE(reject_multiple_root_values) {
    serializer_t serializer;

    auto first = serializer.serialize_bool(true);
    ASSERT_TRUE(first.has_value());

    auto second = serializer.serialize_int(1);
    EXPECT_FALSE(second.has_value());
    EXPECT_FALSE(serializer.valid());
    EXPECT_EQ(serializer.error(), simdjson::TAPE_ERROR);

    auto out = serializer.str();
    EXPECT_FALSE(out.has_value());
    if(!out.has_value()) {
        EXPECT_EQ(out.error(), simdjson::TAPE_ERROR);
    }
}

TEST_CASE(clear_allows_reuse) {
    serializer_t serializer;

    auto first = serializer.serialize_bool(true);
    ASSERT_TRUE(first.has_value());

    serializer.clear();

    std::vector<int> value{7, 9};
    auto second = serde::serialize(serializer, value);
    ASSERT_TRUE(second.has_value());

    auto out = serializer.str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([7,9])");
}

};  // TEST_SUITE(serde_simdjson)

}  // namespace

}  // namespace serde::testing
