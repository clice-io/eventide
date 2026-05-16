#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/dyn/dyn.h"

namespace kota::codec {

namespace {

TEST_SUITE(serde_content_serializer) {

TEST_CASE(serialize_leaf_values) {
    auto null_r = dyn::to_content(nullptr);
    ASSERT_TRUE(null_r.has_value());
    EXPECT_TRUE(null_r->is_null());

    auto bool_r = dyn::to_content(true);
    ASSERT_TRUE(bool_r.has_value());
    EXPECT_EQ(bool_r->as_bool(), true);

    auto int_r = dyn::to_content(42);
    ASSERT_TRUE(int_r.has_value());
    EXPECT_EQ(int_r->as_int(), 42);

    auto str_r = dyn::to_content(std::string("hello"));
    ASSERT_TRUE(str_r.has_value());
    EXPECT_EQ(str_r->as_string(), "hello");
}

TEST_CASE(serialize_array) {
    std::vector<int> vec = {1, 2};
    auto result = dyn::to_content(vec);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_array());
    EXPECT_EQ(result->as_array().size(), 2);
    EXPECT_EQ(result->as_array()[0].as_int(), 1);
    EXPECT_EQ(result->as_array()[1].as_int(), 2);
}

TEST_CASE(serialize_object) {
    std::map<std::string, int> m;
    m["a"] = 1;
    m["b"] = 2;
    auto result = dyn::to_content(m);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_object());

    const auto& obj = result->as_object();
    EXPECT_EQ(obj.size(), 2);
    EXPECT_EQ(obj.at("a").as_int(), 1);
    EXPECT_EQ(obj.at("b").as_int(), 2);
}

TEST_CASE(serialize_element_with_dom_subtree) {
    dyn::Object subtree;
    subtree.insert("k", dyn::Value(std::int64_t(9)));

    std::vector<dyn::Value> vec;
    vec.push_back(dyn::Value(std::move(subtree)));

    auto result = dyn::to_content(vec);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_array());
    const auto& array = result->as_array();
    ASSERT_EQ(array.size(), 1);
    ASSERT_TRUE(array[0].is_object());
    EXPECT_EQ(array[0].as_object().at("k").as_int(), 9);
}

TEST_CASE(codec_serialize_returns_content_value) {
    auto result = dyn::to_content(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->as_int(), 42);
}

TEST_CASE(passthrough_value) {
    dyn::Value original(std::int64_t{99});
    auto result = dyn::to_content(original);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->as_int(), 99);
}

TEST_CASE(passthrough_array) {
    dyn::Array arr;
    arr.push_back(dyn::Value(std::int64_t{1}));
    arr.push_back(dyn::Value(std::int64_t{2}));
    auto result = dyn::to_content(arr);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_array());
    EXPECT_EQ(result->as_array().size(), 2);
}

TEST_CASE(passthrough_object) {
    dyn::Object obj;
    obj.insert("x", dyn::Value(std::int64_t{1}));
    auto result = dyn::to_content(obj);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_object());
    EXPECT_EQ(result->as_object().at("x").as_int(), 1);
}

};  // TEST_SUITE(serde_content_serializer)

}  // namespace

}  // namespace kota::codec
