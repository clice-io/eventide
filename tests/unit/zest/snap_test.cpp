#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "fixtures/schema/common.h"
#include "kota/codec/content/serializer.h"
#include "kota/codec/json/json.h"
#include "kota/zest/snap.h"
#include "kota/zest/zest.h"

namespace kota::zest {

namespace {

namespace fs = std::filesystem;

using codec::json::to_json;
using meta::fixtures::Person;
using meta::fixtures::Point2i;

TEST_SUITE(zest_snap) {

TEST_CASE(snapshot_scalar) {
    EXPECT_SNAPSHOT(42);
    EXPECT_SNAPSHOT(std::string("hello world"));
}

TEST_CASE(snapshot_json_struct) {
    Person p{.name = "Alice", .age = 30, .addr = {.city = "Tokyo", .zip = 100}};
    EXPECT_JSON_SNAPSHOT(p);
}

TEST_CASE(snapshot_json_vector) {
    std::vector<Point2i> points = {{1, 2}, {3, 4}, {5, 6}};
    EXPECT_JSON_SNAPSHOT(points);
}

TEST_CASE(snapshot_yaml_struct) {
    Person p{.name = "Bob", .age = 25, .addr = {.city = "Osaka", .zip = 530}};
    EXPECT_YAML_SNAPSHOT(p);
}

TEST_CASE(snapshot_inline_basic) {
    EXPECT_INLINE_SNAPSHOT(42, "42");
    EXPECT_INLINE_SNAPSHOT(std::string("test"), "test");
}

TEST_CASE(snapshot_multiple_per_test) {
    EXPECT_SNAPSHOT(100);
    EXPECT_SNAPSHOT(200);
    EXPECT_SNAPSHOT(300);
}

TEST_CASE(prettify_json_basic) {
    auto result = snap::prettify_json(R"({"name":"Alice","age":30})");
    auto expected = R"({
  "name": "Alice",
  "age": 30
})";
    EXPECT_EQ(result, expected);
}

TEST_CASE(prettify_json_nested) {
    auto result = snap::prettify_json(R"({"a":{"b":1},"c":[1,2,3]})");
    auto expected = R"({
  "a": {
    "b": 1
  },
  "c": [
    1,
    2,
    3
  ]
})";
    EXPECT_EQ(result, expected);
}

TEST_CASE(prettify_json_empty_containers) {
    EXPECT_EQ(snap::prettify_json("{}"), "{}");
    EXPECT_EQ(snap::prettify_json("[]"), "[]");
    EXPECT_EQ(snap::prettify_json(R"({"a":{},"b":[]})"), R"({
  "a": {},
  "b": []
})");
}

TEST_CASE(prettify_json_strings_with_special_chars) {
    auto result = snap::prettify_json(R"({"msg":"hello \"world\""})");
    auto expected = R"({
  "msg": "hello \"world\""
})";
    EXPECT_EQ(result, expected);
}

TEST_CASE(yaml_basic_object) {
    codec::content::Value val(codec::content::Object{
        {"name", codec::content::Value("Alice")},
        {"age", codec::content::Value(30)},
    });
    auto yaml = snap::value_to_yaml(val);
    auto expected = R"(name: Alice
age: 30
)";
    EXPECT_EQ(yaml, expected);
}

TEST_CASE(yaml_nested_object) {
    codec::content::Value val(codec::content::Object{
        {"person",
         codec::content::Value(codec::content::Object{
             {"name", codec::content::Value("Bob")},
             {"city", codec::content::Value("Osaka")},
         })},
    });
    auto yaml = snap::value_to_yaml(val);
    auto expected = R"(person:
  name: Bob
  city: Osaka
)";
    EXPECT_EQ(yaml, expected);
}

TEST_CASE(yaml_array) {
    codec::content::Value val(codec::content::Array{
        codec::content::Value(1),
        codec::content::Value(2),
        codec::content::Value(3),
    });
    auto yaml = snap::value_to_yaml(val);
    auto expected = R"(- 1
- 2
- 3
)";
    EXPECT_EQ(yaml, expected);
}

TEST_CASE(yaml_empty_containers) {
    EXPECT_EQ(snap::value_to_yaml(codec::content::Value(codec::content::Object{})), "{}\n");
    EXPECT_EQ(snap::value_to_yaml(codec::content::Value(codec::content::Array{})), "[]\n");
}

TEST_CASE(yaml_string_quoting) {
    codec::content::Value val(codec::content::Object{
        {"plain", codec::content::Value("hello")},
        {"needs_quote", codec::content::Value("true")},
        {"with_colon", codec::content::Value("key: value")},
        {"empty", codec::content::Value("")},
    });
    auto yaml = snap::value_to_yaml(val);
    auto expected = R"(plain: hello
needs_quote: "true"
with_colon: "key: value"
empty: ""
)";
    EXPECT_EQ(yaml, expected);
}

TEST_CASE(snapshot_path_single) {
    auto path = snap::snapshot_path("/home/user/tests/test.cpp", "suite", "case", 0, std::nullopt);
    EXPECT_EQ(path.filename().string(), "suite__case.snap");
    EXPECT_TRUE(path.parent_path().filename().string() == "__snapshots__");
}

TEST_CASE(snapshot_path_multi) {
    auto path = snap::snapshot_path("/home/user/tests/test.cpp", "suite", "case", 1, std::nullopt);
    EXPECT_EQ(path.filename().string(), "suite__case@2.snap");
}

TEST_CASE(snapshot_path_glob) {
    snap::GlobContext glob_ctx{"input1"};
    auto path =
        snap::snapshot_path("/home/user/tests/test.cpp", "suite", "case", 0, glob_ctx);
    EXPECT_EQ(path.filename().string(), "suite__case__input1.snap");
}

};  // TEST_SUITE

}  // namespace

}  // namespace kota::zest
