#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/debug/encode.h"

namespace kota::codec::debug {

namespace {

enum class Color { Red, Green, Blue };

struct Point {
    int x;
    int y;
};

struct Person {
    std::string name;
    int age;
    std::optional<std::string> email;
};

}  // namespace

}  // namespace kota::codec::debug

namespace kota::codec::debug {

namespace {

TEST_SUITE(debug_encode) {

TEST_CASE(bool_values) {
    EXPECT_EQ(to_string(true), "true");
    EXPECT_EQ(to_string(false), "false");
};

TEST_CASE(integer_values) {
    EXPECT_EQ(to_string(42), "42");
    EXPECT_EQ(to_string(-1), "-1");
    EXPECT_EQ(to_string(std::uint64_t{100}), "100");
};

TEST_CASE(float_values) {
    EXPECT_EQ(to_string(3.14), "3.14");
    EXPECT_EQ(to_string(0.0), "0");
    EXPECT_EQ(to_string(1.0), "1");
    EXPECT_EQ(to_string(-2.5), "-2.5");
};

TEST_CASE(float32_values) {
    EXPECT_EQ(to_string(1.0f), "1");
    EXPECT_EQ(to_string(3.14f), "3.140000104904175");
};

TEST_CASE(float_special) {
    EXPECT_EQ(to_string(std::numeric_limits<double>::quiet_NaN()), "nan");
    EXPECT_EQ(to_string(std::numeric_limits<double>::infinity()), "inf");
    EXPECT_EQ(to_string(-std::numeric_limits<double>::infinity()), "-inf");
};

TEST_CASE(string_values) {
    EXPECT_EQ(to_string(std::string("hello")), R"("hello")");
    EXPECT_EQ(to_string(std::string("")), R"("")");
};

TEST_CASE(string_escaping) {
    EXPECT_EQ(to_string(std::string("a\"b")), R"("a\"b")");
    EXPECT_EQ(to_string(std::string("a\\b")), R"("a\\b")");
    EXPECT_EQ(to_string(std::string("a\nb")), R"("a\nb")");
    EXPECT_EQ(to_string(std::string("a\tb")), R"("a\tb")");
    EXPECT_EQ(to_string(std::string("a\rb")), R"("a\rb")");
    EXPECT_EQ(to_string(std::string(1, '\0')), R"("\0")");
};

TEST_CASE(char_values) {
    EXPECT_EQ(to_string('a'), "'a'");
    EXPECT_EQ(to_string('\''), R"('\'')");
    EXPECT_EQ(to_string('\\'), R"('\\')");
    EXPECT_EQ(to_string('\n'), R"('\n')");
    EXPECT_EQ(to_string('\0'), R"('\0')");
};

TEST_CASE(bytes_values) {
    std::byte data[] = {std::byte{0x00}, std::byte{0xab}, std::byte{0xff}};
    std::span<const std::byte> bytes(data, 3);
    EXPECT_EQ(to_string(bytes), "[0x00, 0xab, 0xff]");
};

TEST_CASE(bytes_empty) {
    std::span<const std::byte> bytes;
    EXPECT_EQ(to_string(bytes), "[]");
};

TEST_CASE(null_optional) {
    std::optional<int> opt;
    EXPECT_EQ(to_string(opt), "null");
};

TEST_CASE(some_optional) {
    std::optional<int> opt = 42;
    EXPECT_EQ(to_string(opt), "42");
};

TEST_CASE(enum_value) {
    EXPECT_EQ(to_string(Color::Red), "Color::Red");
    EXPECT_EQ(to_string(Color::Blue), "Color::Blue");
};

TEST_CASE(enum_out_of_range) {
    auto v = static_cast<Color>(99);
    EXPECT_EQ(to_string(v), "Color::99");
};

TEST_CASE(vector) {
    std::vector<int> v = {1, 2, 3};
    EXPECT_EQ(to_string(v), "[1, 2, 3]");
};

TEST_CASE(empty_vector) {
    std::vector<int> v;
    EXPECT_EQ(to_string(v), "[]");
};

TEST_CASE(set_uses_braces) {
    std::set<int> s = {1, 2, 3};
    EXPECT_EQ(to_string(s), "{1, 2, 3}");
};

TEST_CASE(empty_set) {
    std::set<int> s;
    EXPECT_EQ(to_string(s), "{}");
};

TEST_CASE(map) {
    std::map<std::string, int> m = {
        {"a", 1},
        {"b", 2}
    };
    EXPECT_EQ(to_string(m), R"({"a": 1, "b": 2})");
};

TEST_CASE(empty_map) {
    std::map<std::string, int> m;
    EXPECT_EQ(to_string(m), "{}");
};

TEST_CASE(map_int_keys) {
    std::map<int, std::string> m = {
        {1, "one"},
        {2, "two"}
    };
    EXPECT_EQ(to_string(m), R"({1: "one", 2: "two"})");
};

TEST_CASE(tuple) {
    auto t = std::make_tuple(1, std::string("two"), true);
    EXPECT_EQ(to_string(t), R"((1, "two", true))");
};

TEST_CASE(single_element_tuple) {
    auto t = std::make_tuple(42);
    EXPECT_EQ(to_string(t), "(42,)");
};

TEST_CASE(empty_tuple) {
    auto t = std::make_tuple();
    EXPECT_EQ(to_string(t), "()");
};

TEST_CASE(simple_struct) {
    Point p{10, 20};
    EXPECT_EQ(to_string(p), "Point { x: 10, y: 20 }");
};

TEST_CASE(nested_struct) {
    Person p{"Alice", 30, "alice@example.com"};
    EXPECT_EQ(to_string(p), R"(Person { name: "Alice", age: 30, email: "alice@example.com" })");
};

TEST_CASE(struct_with_null_optional) {
    Person p{"Bob", 25, std::nullopt};
    EXPECT_EQ(to_string(p), R"(Person { name: "Bob", age: 25, email: null })");
};

TEST_CASE(variant) {
    std::variant<int, std::string> v1 = 42;
    EXPECT_EQ(to_string(v1), "42");
    std::variant<int, std::string> v2 = std::string("hello");
    EXPECT_EQ(to_string(v2), R"("hello")");
};

TEST_CASE(nested_containers) {
    std::vector<std::vector<int>> v = {
        {1, 2},
        {3, 4}
    };
    EXPECT_EQ(to_string(v), "[[1, 2], [3, 4]]");
};

TEST_CASE(raw_pointer) {
    int* null_ptr = nullptr;
    EXPECT_EQ(to_string(null_ptr), "null");
    int x = 42;
    auto result = to_string(&x);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->starts_with("0x"));
};

TEST_CASE(unique_ptr_null) {
    std::unique_ptr<int> p;
    EXPECT_EQ(to_string(p), "null");
};

TEST_CASE(unique_ptr_value) {
    auto p = std::make_unique<int>(42);
    auto result = to_string(p);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->starts_with("0x"));
};

TEST_CASE(shared_ptr_null) {
    std::shared_ptr<int> p;
    EXPECT_EQ(to_string(p), "null");
};

TEST_CASE(shared_ptr_value) {
    auto p = std::make_shared<int>(42);
    auto result = to_string(p);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->starts_with("0x"));
};

TEST_CASE(weak_ptr_valid) {
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;
    auto result = to_string(wp);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->starts_with("0x"));
};

TEST_CASE(weak_ptr_expired) {
    std::weak_ptr<int> wp;
    {
        auto sp = std::make_shared<int>(42);
        wp = sp;
    }
    EXPECT_EQ(to_string(wp), "null");
};

TEST_CASE(expected_value) {
    std::expected<int, std::string> e = 42;
    EXPECT_EQ(to_string(e), "42");
};

TEST_CASE(expected_error) {
    std::expected<int, std::string> e = std::unexpected(std::string("oops"));
    EXPECT_EQ(to_string(e), R"("oops")");
};

TEST_CASE(expected_void_value) {
    std::expected<void, std::string> e;
    EXPECT_EQ(to_string(e), "null");
};

TEST_CASE(expected_void_error) {
    std::expected<void, std::string> e = std::unexpected(std::string("fail"));
    EXPECT_EQ(to_string(e), R"("fail")");
};

};  // TEST_SUITE(debug_encode)

TEST_SUITE(debug_encode_pretty) {

TEST_CASE(simple_struct) {
    Point p{10, 20};
    auto expected = R"(Point {
    x: 10,
    y: 20,
})";
    EXPECT_EQ(to_string(p, true), expected);
};

TEST_CASE(nested_struct) {
    Person p{"Alice", 30, std::nullopt};
    auto expected = R"(Person {
    name: "Alice",
    age: 30,
    email: null,
})";
    EXPECT_EQ(to_string(p, true), expected);
};

TEST_CASE(vector) {
    std::vector<int> v = {1, 2, 3};
    auto expected = R"([
    1,
    2,
    3,
])";
    EXPECT_EQ(to_string(v, true), expected);
};

TEST_CASE(empty_containers) {
    EXPECT_EQ(to_string(std::vector<int>{}, true), "[]");
    EXPECT_EQ(to_string(std::set<int>{}, true), "{}");
    EXPECT_EQ(to_string(std::map<std::string, int>{}, true), "{}");
};

TEST_CASE(nested_pretty) {
    std::vector<Point> v = {
        {1, 2},
        {3, 4}
    };
    auto expected = R"([
    Point {
        x: 1,
        y: 2,
    },
    Point {
        x: 3,
        y: 4,
    },
])";
    EXPECT_EQ(to_string(v, true), expected);
};

TEST_CASE(map_pretty) {
    std::map<std::string, int> m = {
        {"a", 1},
        {"b", 2}
    };
    auto expected = R"({
    "a": 1,
    "b": 2,
})";
    EXPECT_EQ(to_string(m, true), expected);
};

TEST_CASE(set_pretty) {
    std::set<int> s = {1, 2, 3};
    auto expected = R"({
    1,
    2,
    3,
})";
    EXPECT_EQ(to_string(s, true), expected);
};

TEST_CASE(tuple_pretty) {
    auto t = std::make_tuple(1, std::string("two"), true);
    auto expected = R"((
    1,
    "two",
    true,
))";
    EXPECT_EQ(to_string(t, true), expected);
};

TEST_CASE(expected_pretty) {
    std::expected<Point, std::string> e = Point{1, 2};
    auto expected = R"(Point {
    x: 1,
    y: 2,
})";
    EXPECT_EQ(to_string(e, true), expected);
};

TEST_CASE(primitives_unchanged) {
    EXPECT_EQ(to_string(42, true), "42");
    EXPECT_EQ(to_string(true, true), "true");
    EXPECT_EQ(to_string(std::string("hi"), true), R"("hi")");
};

};  // TEST_SUITE(debug_encode_pretty)

}  // namespace
}  // namespace kota::codec::debug
