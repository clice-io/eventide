#include <cstddef>
#include <optional>
#include <string>

#include "eventide/zest/zest.h"
#include "eventide/serde/json/error.h"

// Tests for the backend-agnostic serde_error<Kind> API.
// json::error_kind is used as a concrete Kind since the JSON error header
// is always available (it has no external library dependency beyond the
// eventide::serde target that is unconditionally linked to unit_tests).

namespace eventide::serde {

namespace {

using json_error = json::error;
using json::error_kind;

// ---------------------------------------------------------------------------
// Factory methods
// ---------------------------------------------------------------------------

TEST_SUITE(serde_error_factory_methods) {

TEST_CASE(default_constructed_kind_is_ok) {
    json_error e{};
    EXPECT_EQ(e.kind, error_kind::ok);
}

TEST_CASE(construct_from_kind) {
    json_error e{error_kind::parse_error};
    EXPECT_EQ(e.kind, error_kind::parse_error);
    EXPECT_EQ(e.message(), "parse error");
}

TEST_CASE(missing_field) {
    auto e = json_error::missing_field("name");
    EXPECT_EQ(e.kind, error_kind::type_mismatch);
    EXPECT_EQ(e.message(), "missing required field 'name'");
}

TEST_CASE(unknown_field) {
    auto e = json_error::unknown_field("extra");
    EXPECT_EQ(e.kind, error_kind::type_mismatch);
    EXPECT_EQ(e.message(), "unknown field 'extra'");
}

TEST_CASE(duplicate_field) {
    auto e = json_error::duplicate_field("id");
    EXPECT_EQ(e.kind, error_kind::type_mismatch);
    EXPECT_EQ(e.message(), "duplicate field 'id'");
}

TEST_CASE(invalid_type) {
    auto e = json_error::invalid_type("string", "int");
    EXPECT_EQ(e.kind, error_kind::type_mismatch);
    EXPECT_EQ(e.message(), "invalid type: expected string, got int");
}

TEST_CASE(invalid_length) {
    auto e = json_error::invalid_length(3, 5);
    EXPECT_EQ(e.kind, error_kind::type_mismatch);
    EXPECT_EQ(e.message(), "invalid length: expected 3, got 5");
}

TEST_CASE(custom_default_kind) {
    auto e = json_error::custom("something went wrong");
    EXPECT_EQ(e.kind, error_kind::type_mismatch);
    EXPECT_EQ(e.message(), "something went wrong");
}

TEST_CASE(custom_explicit_kind) {
    auto e = json_error::custom(error_kind::io_error, "connection lost");
    EXPECT_EQ(e.kind, error_kind::io_error);
    EXPECT_EQ(e.message(), "connection lost");
}

TEST_CASE(empty_message_custom) {
    auto e = json_error::custom("");
    EXPECT_EQ(e.kind, error_kind::type_mismatch);
    EXPECT_EQ(std::string(e.message()), "");
}

};  // TEST_SUITE(serde_error_factory_methods)

// ---------------------------------------------------------------------------
// Path manipulation
// ---------------------------------------------------------------------------

TEST_SUITE(serde_error_path) {

TEST_CASE(no_path_on_fresh_error) {
    auto e = json_error::missing_field("x");
    EXPECT_EQ(e.format_path(), "");
}

TEST_CASE(prepend_field_single) {
    auto e = json_error::missing_field("x");
    e.prepend_field("outer");
    EXPECT_EQ(e.format_path(), "outer");
}

TEST_CASE(prepend_index_single) {
    auto e = json_error::invalid_type("int", "string");
    e.prepend_index(3);
    EXPECT_EQ(e.format_path(), "[3]");
}

TEST_CASE(prepend_field_then_index) {
    // After: field[idx] — field was prepended first, then index prepended before it
    // prepend_index puts the index at position 0, field moves to position 1
    // format_path: [idx].field  (index has no dot separator, field after index gets dot)
    // Actually let's verify: first prepend "scores" then prepend 1 → path = [1, "scores"]
    // format_path iterates: i=0 index→"[1]", i=1 field (i>0)→".scores"  → "[1].scores"
    // That is the sequence produced by the deserializer when it goes array→field.
    // For the reverse (field wrapping array element), field is added last (outermost):
    // prepend_index(1) first, then prepend_field("scores"):
    //   path = ["scores", 1]  → "scores[1]"
    auto e = json_error::invalid_type("int", "string");
    e.prepend_index(1);
    e.prepend_field("scores");
    EXPECT_EQ(e.format_path(), "scores[1]");
}

TEST_CASE(multiple_prepend_field_builds_dotted_path) {
    auto e = json_error::missing_field("leaf");
    e.prepend_field("inner");
    e.prepend_field("outer");
    EXPECT_EQ(e.format_path(), "outer.inner");
}

TEST_CASE(prepend_field_preserves_message) {
    auto e = json_error::missing_field("x");
    e.prepend_field("ctx");
    EXPECT_EQ(e.message(), "missing required field 'x'");
}

TEST_CASE(prepend_index_on_kind_only_error_preserves_kind_message) {
    // When prepend is called on an error with no detail, ensure_detail copies
    // error_message(kind) as the stored message.
    json_error e{error_kind::type_mismatch};
    e.prepend_index(0);
    EXPECT_EQ(e.format_path(), "[0]");
    EXPECT_EQ(e.message(), "type mismatch");
}

TEST_CASE(format_path_index_at_start_no_leading_dot) {
    auto e = json_error::custom("err");
    e.prepend_field("b");
    e.prepend_index(2);
    e.prepend_field("a");
    // path order: ["a", 2, "b"]
    // i=0 field "a" (i==0, no dot) → "a"
    // i=1 index 2 → "[2]"
    // i=2 field "b" (i>0, add dot) → ".b"
    // result: "a[2].b"
    EXPECT_EQ(e.format_path(), "a[2].b");
}

};  // TEST_SUITE(serde_error_path)

// ---------------------------------------------------------------------------
// Location
// ---------------------------------------------------------------------------

TEST_SUITE(serde_error_location) {

TEST_CASE(no_location_by_default) {
    auto e = json_error::missing_field("f");
    EXPECT_FALSE(e.location().has_value());
}

TEST_CASE(set_and_get_location) {
    auto e = json_error::invalid_type("int", "string");
    e.set_location({10, 5, 42});
    ASSERT_TRUE(e.location().has_value());
    EXPECT_EQ(e.location()->line, 10u);
    EXPECT_EQ(e.location()->column, 5u);
    EXPECT_EQ(e.location()->byte_offset, 42u);
}

TEST_CASE(set_location_on_kind_only_error) {
    json_error e{error_kind::parse_error};
    e.set_location({1, 1, 0});
    ASSERT_TRUE(e.location().has_value());
    EXPECT_EQ(e.location()->line, 1u);
}

TEST_CASE(location_independent_of_path) {
    auto e = json_error::missing_field("x");
    e.prepend_field("obj");
    e.set_location({7, 3, 20});
    EXPECT_EQ(e.format_path(), "obj");
    ASSERT_TRUE(e.location().has_value());
    EXPECT_EQ(e.location()->line, 7u);
    EXPECT_EQ(e.location()->column, 3u);
}

};  // TEST_SUITE(serde_error_location)

// ---------------------------------------------------------------------------
// Formatting (to_string / message)
// ---------------------------------------------------------------------------

TEST_SUITE(serde_error_formatting) {

TEST_CASE(message_returns_raw_message) {
    auto e = json_error::custom("raw msg");
    EXPECT_EQ(e.message(), "raw msg");
}

TEST_CASE(message_from_kind_only) {
    json_error e{error_kind::type_mismatch};
    EXPECT_EQ(e.message(), "type mismatch");
}

TEST_CASE(to_string_no_path_no_location) {
    auto e = json_error::custom("oops");
    EXPECT_EQ(e.to_string(), "oops");
}

TEST_CASE(to_string_with_path_no_location) {
    auto e = json_error::missing_field("name");
    e.prepend_field("user");
    EXPECT_EQ(e.to_string(), "missing required field 'name' at user");
}

TEST_CASE(to_string_with_location_no_path) {
    auto e = json_error::invalid_type("int", "string");
    e.set_location({3, 8, 0});
    EXPECT_EQ(e.to_string(), "invalid type: expected int, got string (line 3, column 8)");
}

TEST_CASE(to_string_with_path_and_location) {
    auto e = json_error::invalid_type("int", "string");
    e.prepend_field("age");
    e.set_location({5, 12, 0});
    EXPECT_EQ(e.to_string(), "invalid type: expected int, got string at age (line 5, column 12)");
}

TEST_CASE(to_string_kind_only_no_detail) {
    json_error e{error_kind::parse_error};
    EXPECT_EQ(e.to_string(), "parse error");
}

};  // TEST_SUITE(serde_error_formatting)

// ---------------------------------------------------------------------------
// Comparison operators
// ---------------------------------------------------------------------------

TEST_SUITE(serde_error_comparison) {

TEST_CASE(equal_same_kind) {
    auto a = json_error::missing_field("x");
    auto b = json_error::unknown_field("y");
    // Both have kind type_mismatch
    EXPECT_TRUE(a == b);
}

TEST_CASE(not_equal_different_kinds) {
    auto a = json_error::custom(error_kind::io_error, "io");
    auto b = json_error::custom(error_kind::parse_error, "parse");
    EXPECT_FALSE(a == b);
}

TEST_CASE(equal_to_kind_value) {
    auto e = json_error::missing_field("f");
    EXPECT_TRUE(e == error_kind::type_mismatch);
    EXPECT_FALSE(e == error_kind::parse_error);
}

TEST_CASE(default_constructed_equals_ok) {
    json_error e{};
    EXPECT_TRUE(e == error_kind::ok);
}

TEST_CASE(copy_preserves_equality) {
    auto a = json_error::missing_field("x");
    auto b = a;  // copy
    EXPECT_TRUE(a == b);
    EXPECT_EQ(a.kind, b.kind);
    EXPECT_EQ(a.message(), b.message());
}

TEST_CASE(copy_preserves_path_and_location) {
    auto a = json_error::invalid_type("int", "string");
    a.prepend_field("field");
    a.set_location({2, 4, 10});
    auto b = a;
    EXPECT_EQ(b.format_path(), "field");
    ASSERT_TRUE(b.location().has_value());
    EXPECT_EQ(b.location()->line, 2u);
    EXPECT_EQ(b.location()->column, 4u);
    EXPECT_EQ(b.location()->byte_offset, 10u);
}

};  // TEST_SUITE(serde_error_comparison)

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_SUITE(serde_error_edge_cases) {

TEST_CASE(minimal_error_kind_only) {
    json_error e{error_kind::unknown};
    EXPECT_EQ(e.kind, error_kind::unknown);
    EXPECT_FALSE(e.location().has_value());
    EXPECT_EQ(e.format_path(), "");
}

TEST_CASE(move_construction) {
    auto a = json_error::missing_field("x");
    a.prepend_field("ctx");
    a.set_location({1, 2, 3});
    auto b = std::move(a);
    EXPECT_EQ(b.kind, error_kind::type_mismatch);
    EXPECT_EQ(b.message(), "missing required field 'x'");
    EXPECT_EQ(b.format_path(), "ctx");
    ASSERT_TRUE(b.location().has_value());
    EXPECT_EQ(b.location()->line, 1u);
}

TEST_CASE(move_assignment) {
    auto a = json_error::custom(error_kind::io_error, "io err");
    json_error b{};
    b = std::move(a);
    EXPECT_EQ(b.kind, error_kind::io_error);
    EXPECT_EQ(b.message(), "io err");
}

TEST_CASE(format_path_empty_when_no_detail) {
    json_error e{error_kind::type_mismatch};
    EXPECT_EQ(e.format_path(), "");
}

TEST_CASE(location_nullopt_when_no_detail) {
    json_error e{error_kind::type_mismatch};
    EXPECT_FALSE(e.location().has_value());
}

TEST_CASE(multiple_index_prepends) {
    auto e = json_error::custom("err");
    e.prepend_index(2);
    e.prepend_index(1);
    e.prepend_index(0);
    // path order: [0, 1, 2]  → "[0][1][2]"
    EXPECT_EQ(e.format_path(), "[0][1][2]");
}

};  // TEST_SUITE(serde_error_edge_cases)

}  // namespace

}  // namespace eventide::serde
