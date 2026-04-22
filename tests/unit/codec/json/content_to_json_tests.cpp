#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"

namespace kota::codec {

namespace {

namespace js = kota::codec::json;

TEST_SUITE(content_to_json) {

// ---------------------------------------------------------------------------
// Leaf values
// ---------------------------------------------------------------------------

TEST_CASE(null_value) {
    content::Value v(nullptr);
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "null");
}

TEST_CASE(bool_true) {
    content::Value v(true);
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "true");
}

TEST_CASE(bool_false) {
    content::Value v(false);
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "false");
}

TEST_CASE(signed_int_positive) {
    content::Value v(std::int64_t{42});
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "42");
}

TEST_CASE(signed_int_negative) {
    content::Value v(std::int64_t{-100});
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "-100");
}

TEST_CASE(signed_int_zero) {
    content::Value v(std::int64_t{0});
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "0");
}

TEST_CASE(signed_int_min) {
    content::Value v(std::numeric_limits<std::int64_t>::min());
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "-9223372036854775808");
}

TEST_CASE(signed_int_max) {
    content::Value v(std::numeric_limits<std::int64_t>::max());
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "9223372036854775807");
}

TEST_CASE(unsigned_int) {
    content::Value v(std::uint64_t{123});
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "123");
}

TEST_CASE(unsigned_int_max) {
    content::Value v(std::numeric_limits<std::uint64_t>::max());
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "18446744073709551615");
}

TEST_CASE(floating_point) {
    content::Value v(3.14);
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "3.14");
}

TEST_CASE(floating_point_zero) {
    content::Value v(0.0);
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "0.0");
}

TEST_CASE(floating_point_negative) {
    content::Value v(-2.5);
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "-2.5");
}

TEST_CASE(floating_point_nan) {
    content::Value v(std::numeric_limits<double>::quiet_NaN());
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "null");
}

TEST_CASE(floating_point_inf) {
    content::Value v(std::numeric_limits<double>::infinity());
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "null");
}

TEST_CASE(string_simple) {
    content::Value v(std::string("hello"));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"("hello")");
}

TEST_CASE(string_empty) {
    content::Value v(std::string(""));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"("")");
}

TEST_CASE(string_with_escapes) {
    content::Value v(std::string("a\"b\\c\n"));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"("a\"b\\c\n")");
}

TEST_CASE(string_with_control_chars) {
    content::Value v(std::string("tab\there"));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"("tab\there")");
}

// ---------------------------------------------------------------------------
// Arrays
// ---------------------------------------------------------------------------

TEST_CASE(empty_array) {
    content::Array arr;
    content::Value v(std::move(arr));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "[]");
}

TEST_CASE(array_single_element) {
    content::Array arr;
    arr.push_back(content::Value(std::int64_t{1}));
    content::Value v(std::move(arr));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "[1]");
}

TEST_CASE(array_multiple_elements) {
    content::Array arr;
    arr.push_back(content::Value(std::int64_t{1}));
    arr.push_back(content::Value(std::int64_t{2}));
    arr.push_back(content::Value(std::int64_t{3}));
    content::Value v(std::move(arr));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "[1,2,3]");
}

TEST_CASE(array_mixed_types) {
    content::Array arr;
    arr.push_back(content::Value(nullptr));
    arr.push_back(content::Value(true));
    arr.push_back(content::Value(std::int64_t{42}));
    arr.push_back(content::Value(3.14));
    arr.push_back(content::Value(std::string("hi")));
    content::Value v(std::move(arr));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"([null,true,42,3.14,"hi"])");
}

TEST_CASE(array_nested) {
    content::Array inner;
    inner.push_back(content::Value(std::int64_t{1}));
    inner.push_back(content::Value(std::int64_t{2}));
    content::Array outer;
    outer.push_back(content::Value(std::move(inner)));
    outer.push_back(content::Value(std::int64_t{3}));
    content::Value v(std::move(outer));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "[[1,2],3]");
}

TEST_CASE(array_direct) {
    content::Array arr;
    arr.push_back(content::Value(std::int64_t{10}));
    arr.push_back(content::Value(std::int64_t{20}));
    auto result = js::to_string(arr);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "[10,20]");
}

// ---------------------------------------------------------------------------
// Objects
// ---------------------------------------------------------------------------

TEST_CASE(empty_object) {
    content::Object obj;
    content::Value v(std::move(obj));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "{}");
}

TEST_CASE(object_single_field) {
    content::Object obj;
    obj.insert("x", content::Value(std::int64_t{1}));
    content::Value v(std::move(obj));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"x":1})");
}

TEST_CASE(object_multiple_fields) {
    content::Object obj;
    obj.insert("name", content::Value(std::string("alice")));
    obj.insert("age", content::Value(std::int64_t{30}));
    obj.insert("active", content::Value(true));
    content::Value v(std::move(obj));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"name":"alice","age":30,"active":true})");
}

TEST_CASE(object_nested) {
    content::Object inner;
    inner.insert("a", content::Value(std::int64_t{1}));
    content::Object outer;
    outer.insert("inner", content::Value(std::move(inner)));
    outer.insert("b", content::Value(std::int64_t{2}));
    content::Value v(std::move(outer));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"inner":{"a":1},"b":2})");
}

TEST_CASE(object_with_array_field) {
    content::Array arr;
    arr.push_back(content::Value(std::int64_t{1}));
    arr.push_back(content::Value(std::int64_t{2}));
    content::Object obj;
    obj.insert("items", content::Value(std::move(arr)));
    content::Value v(std::move(obj));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"items":[1,2]})");
}

TEST_CASE(object_with_null_field) {
    content::Object obj;
    obj.insert("value", content::Value(nullptr));
    content::Value v(std::move(obj));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"value":null})");
}

TEST_CASE(object_key_needs_escape) {
    content::Object obj;
    obj.insert("key\"with\"quotes", content::Value(std::int64_t{1}));
    content::Value v(std::move(obj));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"key\"with\"quotes":1})");
}

TEST_CASE(object_direct) {
    content::Object obj;
    obj.insert("x", content::Value(std::int64_t{10}));
    auto result = js::to_string(obj);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"x":10})");
}

// ---------------------------------------------------------------------------
// Deep nesting
// ---------------------------------------------------------------------------

TEST_CASE(deep_nesting) {
    content::Object inner;
    inner.insert("val", content::Value(std::int64_t{42}));

    content::Array arr;
    arr.push_back(content::Value(std::move(inner)));
    arr.push_back(content::Value(nullptr));

    content::Object mid;
    mid.insert("list", content::Value(std::move(arr)));
    mid.insert("flag", content::Value(true));

    content::Object root;
    root.insert("data", content::Value(std::move(mid)));
    root.insert("version", content::Value(std::uint64_t{1}));

    content::Value v(std::move(root));
    auto result = js::to_string(v);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, R"({"data":{"list":[{"val":42},null],"flag":true},"version":1})");
}

// ---------------------------------------------------------------------------
// Round-trip: parse JSON -> content::Value -> to_string
// ---------------------------------------------------------------------------

TEST_CASE(round_trip_simple_object) {
    std::string_view input = R"({"a":1,"b":"two","c":true,"d":null})";
    auto parsed = js::parse<content::Value>(input);
    ASSERT_TRUE(parsed.has_value());
    auto output = js::to_string(*parsed);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, input);
}

TEST_CASE(round_trip_nested) {
    std::string_view input = R"({"x":{"y":[1,2,3]},"z":false})";
    auto parsed = js::parse<content::Value>(input);
    ASSERT_TRUE(parsed.has_value());
    auto output = js::to_string(*parsed);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, input);
}

TEST_CASE(round_trip_array_root) {
    std::string_view input = R"([1,"two",true,null,[3,4]])";
    auto parsed = js::parse<content::Value>(input);
    ASSERT_TRUE(parsed.has_value());
    auto output = js::to_string(*parsed);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, input);
}

};  // TEST_SUITE(content_to_json)

}  // namespace

}  // namespace kota::codec
