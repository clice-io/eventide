#include <optional>
#include <utility>

#include "kota/zest/zest.h"
#include "kota/codec/content/content.h"

namespace kota::codec {

namespace {

TEST_SUITE(serde_content_serializer) {

TEST_CASE(serialize_leaf_values) {
    content::Serializer<> s;

    auto null_r = s.serialize_null();
    ASSERT_TRUE(null_r.has_value());
    EXPECT_TRUE(null_r->is_null());

    auto bool_r = s.serialize_bool(true);
    ASSERT_TRUE(bool_r.has_value());
    EXPECT_EQ(bool_r->as_bool(), true);

    auto int_r = s.serialize_int(42);
    ASSERT_TRUE(int_r.has_value());
    EXPECT_EQ(int_r->as_int(), 42);

    auto str_r = s.serialize_str("hello");
    ASSERT_TRUE(str_r.has_value());
    EXPECT_EQ(str_r->as_string(), "hello");
}

TEST_CASE(serialize_array_with_placement) {
    content::Serializer<> s;

    ASSERT_TRUE(s.begin_array(std::nullopt).has_value());
    ASSERT_TRUE(s.serialize_element([&] { return s.serialize_int(1); }).has_value());
    ASSERT_TRUE(s.serialize_element([&] { return s.serialize_int(2); }).has_value());

    auto result = s.end_array();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_array());
    EXPECT_EQ(result->as_array().size(), 2);
    EXPECT_EQ(result->as_array()[0].as_int(), 1);
    EXPECT_EQ(result->as_array()[1].as_int(), 2);
}

TEST_CASE(serialize_object_with_placement) {
    content::Serializer<> s;

    ASSERT_TRUE(s.begin_object(2).has_value());
    ASSERT_TRUE(s.serialize_field("a", [&] { return s.serialize_int(1); }).has_value());
    ASSERT_TRUE(s.serialize_field("b", [&] { return s.serialize_int(2); }).has_value());

    auto result = s.end_object();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_object());

    const auto& obj = result->as_object();
    EXPECT_EQ(obj.size(), 2);
    EXPECT_EQ(obj.at("a").as_int(), 1);
    EXPECT_EQ(obj.at("b").as_int(), 2);
}

TEST_CASE(end_on_empty_stack_returns_invalid_state) {
    content::Serializer<> s;

    auto arr_result = s.end_array();
    ASSERT_FALSE(arr_result.has_value());
    EXPECT_EQ(arr_result.error(), content::error_kind::invalid_state);

    auto obj_result = s.end_object();
    ASSERT_FALSE(obj_result.has_value());
    EXPECT_EQ(obj_result.error(), content::error_kind::invalid_state);
}

TEST_CASE(end_object_on_array_frame_returns_invalid_state) {
    content::Serializer<> s;

    ASSERT_TRUE(s.begin_array(std::nullopt).has_value());
    auto result = s.end_object();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), content::error_kind::invalid_state);
}

TEST_CASE(serialize_element_with_dom_subtree) {
    content::Serializer<> s;

    content::Object subtree;
    subtree.insert("k", content::Value(std::int64_t(9)));

    ASSERT_TRUE(s.begin_array(std::nullopt).has_value());
    ASSERT_TRUE(s.serialize_element([&]() -> content::Serializer<>::result_t<content::Value> {
                     return content::Value(std::move(subtree));
                 }).has_value());

    auto result = s.end_array();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_array());
    const auto& array = result->as_array();
    ASSERT_EQ(array.size(), 1);
    ASSERT_TRUE(array[0].is_object());
    EXPECT_EQ(array[0].as_object().at("k").as_int(), 9);
}

TEST_CASE(codec_serialize_returns_content_value) {
    content::Serializer<> s;

    auto result = codec::serialize(s, 42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->as_int(), 42);
}

};  // TEST_SUITE(serde_content_serializer)

}  // namespace

}  // namespace kota::codec
