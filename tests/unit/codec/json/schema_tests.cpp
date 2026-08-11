#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/attrs.h"
#include "kota/meta/schema.h"
#include "kota/codec/json/schema.h"
#include "kota/codec/macro.h"

namespace kota::meta {

struct json_schema_opaque_root {};

/// A non-reflectable class (raw kind unknown) whose meta::repr resolves to a
/// struct with a defaulted member: the default-annotation pass judges the
/// JSON-resolved representation, not the raw kind.
class json_schema_reprd_root {
public:
    int total() const {
        return total_;
    }

private:
    int total_ = 5;
};

struct json_schema_reprd_repr {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> total = 5;
};

}  // namespace kota::meta

namespace kota::meta {

template <>
constexpr inline bool schema_opaque<kota::meta::json_schema_opaque_root> = true;

template <>
struct repr<kota::meta::json_schema_reprd_root> {
    using type = kota::meta::json_schema_reprd_repr;

    static type to(const kota::meta::json_schema_reprd_root& v) {
        return {.total = v.total()};
    }
};

}  // namespace kota::meta

namespace kota::meta {

namespace {

// ---------------------------------------------------------------------------
// Scalar wrappers
// ---------------------------------------------------------------------------
struct s_bool {
    bool v;
};

struct s_i8 {
    std::int8_t v;
};

struct s_i16 {
    std::int16_t v;
};

struct s_i32 {
    std::int32_t v;
};

struct s_i64 {
    std::int64_t v;
};

struct s_u8 {
    std::uint8_t v;
};

struct s_u16 {
    std::uint16_t v;
};

struct s_u32 {
    std::uint32_t v;
};

struct s_u64 {
    std::uint64_t v;
};

struct s_f32 {
    float v;
};

struct s_f64 {
    double v;
};

struct s_char {
    char v;
};

struct s_str {
    std::string v;
};

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------
enum class color_i8 : std::int8_t { red = 0, green = 1, blue = 2 };
enum class single_enum : std::int32_t { only = 42 };
enum class status : std::int32_t { ok = 0, fail = 1, pending = 2 };
enum class flag_u8 : std::uint8_t { off = 0, on = 1 };
enum class level_i16 : std::int16_t { low = 0, mid = 50, high = 100 };

// ---------------------------------------------------------------------------
// Containers
// ---------------------------------------------------------------------------
struct s_vec_i32 {
    std::vector<std::int32_t> v;
};

struct s_set_i32 {
    std::set<std::int32_t> v;
};

struct s_map_str_i32 {
    std::map<std::string, std::int32_t> v;
};

struct s_vec_vec_i32 {
    std::vector<std::vector<std::int32_t>> v;
};

struct s_map_str_vec_i32 {
    std::map<std::string, std::vector<std::int32_t>> v;
};

// ---------------------------------------------------------------------------
// Tuple / Pair
// ---------------------------------------------------------------------------
struct s_pair {
    std::pair<std::string, std::int32_t> v;
};

struct s_tuple {
    std::tuple<std::int32_t, std::string, bool> v;
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------
struct empty_struct {};

struct single_field {
    std::int32_t x;
};

struct point2d {
    std::int32_t x;
    std::int32_t y;
};

struct with_string {
    std::string name;
    std::int32_t value;
};

// ---------------------------------------------------------------------------
// Nested structs
// ---------------------------------------------------------------------------
struct inner {
    std::int32_t a;
};

struct middle {
    inner i;
    std::string s;
};

struct outer {
    middle m;
    std::int32_t n;
};

// ---------------------------------------------------------------------------
// Struct with enum
// ---------------------------------------------------------------------------
struct with_enum {
    color_i8 c;
    std::string name;
};

// ---------------------------------------------------------------------------
// Optional / pointer
// ---------------------------------------------------------------------------
struct with_optional {
    std::string name;
    std::optional<std::int32_t> age;
};

struct with_unique {
    std::string name;
    std::unique_ptr<std::int32_t> ptr;
};

struct with_shared {
    std::string name;
    std::shared_ptr<std::int32_t> ptr;
};

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------
struct with_default {
    std::string name;
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> count;
};

struct with_skip {
    std::string visible;
    KOTATSU_ANNOTATE(skip = true)
    <std::int32_t> hidden;
};

struct base_fields {
    std::int32_t a;
    std::int32_t b;
};

struct with_flatten {
    KOTATSU_ANNOTATE(flatten = true)
    <base_fields> base;
    std::string extra;
};

struct with_rename {
    KOTATSU_ANNOTATE(rename = "my_field")
    <std::int32_t> x;
    std::string y;
};

struct with_skip_when {
    std::string name;
    KOTATSU_ANNOTATE(skip_if = skip_when::empty)
    <std::vector<std::int32_t>> tags;
    KOTATSU_ANNOTATE(skip_if = skip_when::default_value)
    <std::int32_t> count;
    KOTATSU_ANNOTATE(skip_if = type<pred::empty>)
    <std::string> note;
};

struct casing_child {
    std::int32_t first_value;
};

struct repeated_child_annotation {
    KOTATSU_ANNOTATE(rename_all = casing::lower_camel)
    <casing_child> left;
    KOTATSU_ANNOTATE(rename_all = casing::lower_camel)
    <casing_child> right;
};

// ---------------------------------------------------------------------------
// Variant
// ---------------------------------------------------------------------------
struct var_none {
    std::variant<std::int32_t, std::string> v;
};

struct var_three {
    std::variant<std::int32_t, std::string, bool> v;
};

struct tagged_circle {
    double radius;
};

struct tagged_rect {
    double width;
    double height;
};

KOTATSU_ANNOTATION(root_external_annotation, tagged = true, tag_names = {"integer", "text"});
using root_external_variant =
    annotate<root_external_annotation>::type<std::variant<std::int32_t, std::string>>;

KOTATSU_ANNOTATION(root_internal_annotation, tag = "kind", tag_names = {"circle", "rect"});
using root_internal_variant =
    annotate<root_internal_annotation>::type<std::variant<tagged_circle, tagged_rect>>;

KOTATSU_ANNOTATION(root_adjacent_annotation,
                   tag = "type",
                   content = "value",
                   tag_names = {"integer", "text"});
using root_adjacent_variant =
    annotate<root_adjacent_annotation>::type<std::variant<std::int32_t, std::string>>;

// ---------------------------------------------------------------------------
// Combinations
// ---------------------------------------------------------------------------
struct combo {
    color_i8 color;
    std::optional<std::string> label;
    std::vector<std::int32_t> values;
    std::map<std::string, std::int32_t> attrs;
};

struct nested_combo {
    point2d point;
    color_i8 color;
    std::vector<point2d> points;
    std::map<std::string, point2d> named_points;
};

struct multi_map {
    std::map<std::string, std::int32_t> a;
    std::map<std::string, std::string> b;
};

struct vec_of_struct {
    std::vector<point2d> items;
};

struct deep_inner {
    color_i8 c;
    std::int32_t v;
};

struct deep_middle {
    deep_inner di;
    std::string s;
};

struct deep_outer {
    deep_middle dm;
    std::int32_t n;
};

// ---------------------------------------------------------------------------
// Additional types
// ---------------------------------------------------------------------------
struct all_optional {
    std::optional<std::int32_t> a;
    std::optional<std::string> b;
};

struct all_default {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> x;
    KOTATSU_ANNOTATE(defaulted = true)
    <std::string> y;
};

struct skip_default {
    std::string name;
    KOTATSU_ANNOTATE(skip = true)
    <std::int32_t> hidden;
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> count;
};

struct base_with_opt {
    std::int32_t x;
    std::optional<std::int32_t> y;
};

struct flatten_opt {
    KOTATSU_ANNOTATE(flatten = true)
    <base_with_opt> base;
    std::string tag;
};

struct rename_base {
    KOTATSU_ANNOTATE(rename = "alpha")
    <std::int32_t> a;
    std::int32_t b;
};

struct flatten_rename {
    KOTATSU_ANNOTATE(flatten = true)
    <rename_base> inner;
    std::string extra;
};

struct map_str_struct {
    std::map<std::string, point2d> entries;
};

struct map_str_enum {
    std::map<std::string, color_i8> entries;
};

struct vec_optional {
    std::vector<std::optional<std::int32_t>> v;
};

struct optional_vec {
    std::optional<std::vector<std::int32_t>> v;
};

struct with_pair_field {
    std::pair<std::string, std::int32_t> p;
    std::string name;
};

struct with_tuple_field {
    std::tuple<std::int32_t, bool> t;
    std::string name;
};

struct shared_struct {
    std::string name;
    std::shared_ptr<point2d> point;
};

struct multi_ref {
    point2d a;
    point2d b;
    std::vector<point2d> list;
};

struct vec_enum {
    std::vector<color_i8> colors;
};

struct set_string {
    std::set<std::string> tags;
};

struct optional_struct {
    std::optional<point2d> point;
    std::string name;
};

struct vec_map {
    std::vector<std::map<std::string, std::int32_t>> items;
};

struct map_vec_struct {
    std::map<std::string, std::vector<point2d>> groups;
};

struct trivial_nested {
    point2d p;
    std::int32_t z;
};

struct multi_enum {
    color_i8 c;
    status s;
    std::string label;
};

struct with_flag {
    flag_u8 f;
    std::string name;
};

struct with_level {
    level_i16 l;
    std::int32_t v;
};

struct with_all_ptr {
    std::optional<std::string> opt;
    std::unique_ptr<std::int32_t> uniq;
    std::shared_ptr<bool> shr;
};

struct deep_container {
    std::map<std::string, std::vector<std::map<std::string, std::int32_t>>> data;
};

struct optional_inner {
    std::optional<inner> i;
    std::string name;
};

struct map_of_map {
    std::map<std::string, std::map<std::string, std::int32_t>> m;
};

struct vec_variant {
    std::vector<std::variant<std::int32_t, std::string>> items;
};

struct many_fields {
    std::int32_t a;
    std::int32_t b;
    std::int32_t c;
    std::string d;
    bool e;
    double f;
};

struct set_of_struct {
    std::set<std::int32_t> ids;
    std::string name;
};

// ---------------------------------------------------------------------------
// description fixtures
// ---------------------------------------------------------------------------

struct desc_scalar {
    KOTATSU_ANNOTATE(description = "Number of worker threads.")
    <std::int32_t> threads;
    std::string name;
};

struct desc_optional {
    KOTATSU_ANNOTATE(description = "Optional display label.")
    <std::optional<std::string>> label;
};

struct desc_struct_ref {
    KOTATSU_ANNOTATE(description = "Anchor position.")
    <point2d> anchor;
};

struct desc_base {
    KOTATSU_ANNOTATE(description = "Inherited counter.")
    <std::int32_t> count;
};

struct desc_flatten {
    KOTATSU_ANNOTATE(flatten = true)
    <desc_base> base;
    std::string tag;
};

struct desc_rename {
    KOTATSU_ANNOTATE(rename = "max_size", description = "Maximum size in bytes.")
    <std::int32_t> size;
};

struct desc_default {
    KOTATSU_ANNOTATE(defaulted = true, description = "Retry limit.")
    <std::int32_t> retries;
};

struct desc_shared_ref {
    point2d origin;
    KOTATSU_ANNOTATE(description = "Anchor position.")
    <point2d> anchor;
};

struct desc_tagged_circle {
    KOTATSU_ANNOTATE(description = "Radius in meters.")
    <double> radius;
};

struct desc_tagged_rect {
    double width;
    double height;
};

KOTATSU_ANNOTATION(desc_internal_annotation, tag = "kind", tag_names = {"circle", "rect"});
using desc_internal_variant =
    annotate<desc_internal_annotation>::type<std::variant<desc_tagged_circle, desc_tagged_rect>>;

// ---------------------------------------------------------------------------
// default annotation fixtures
// ---------------------------------------------------------------------------

struct defaults_leaf {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> threads = 4;

    KOTATSU_ANNOTATE(defaulted = true)
    <std::string> name = "worker";
};

struct defaults_root {
    KOTATSU_ANNOTATE(defaulted = true)
    <bool> enabled = true;

    defaults_leaf pool;
    defaults_leaf mirror;

    std::optional<std::int32_t> limit;

    KOTATSU_ANNOTATE(defaulted = true)
    <std::vector<std::int32_t>> ids;
};

struct defaults_skipped {
    KOTATSU_ANNOTATE(skip_if = skip_when::empty)
    <std::vector<std::int32_t>> tags;
};

enum class defaults_level : std::uint8_t { Low = 0, High = 1 };

struct defaults_with_enum {
    KOTATSU_ANNOTATE(defaulted = true)
    <defaults_level> log_level = defaults_level::High;
};

struct defaults_shared_override {
    defaults_leaf pool;
    defaults_leaf mirror = {.threads = 9};
};

struct defaults_engaged {
    std::optional<std::int32_t> limit = 5;
    std::optional<defaults_leaf> anchor = defaults_leaf{};
};

struct defaults_node {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> depth = 1;

    std::unique_ptr<defaults_node> next;
};

struct defaults_ref_sites {
    KOTATSU_ANNOTATE(defaulted = true)
    <defaults_leaf> pool;

    KOTATSU_ANNOTATE(defaulted = true)
    <defaults_leaf> mirror = {defaults_leaf{.threads = 9}};
};

struct defaults_alt_a {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> depth = 3;

    std::string name;
};

struct defaults_alt_b {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> other = 9;
};

KOTATSU_ANNOTATION(defaults_internal_annotation, tag = "kind", tag_names = {"a", "b"});
using defaults_internal_variant =
    annotate<defaults_internal_annotation>::type<std::variant<defaults_alt_a, defaults_alt_b>>;

struct defaults_variant_holder {
    defaults_internal_variant shape;
};

KOTATSU_ANNOTATION(defaults_adjacent_annotation,
                   tag = "type",
                   content = "value",
                   tag_names = {"a", "b"});
using defaults_adjacent_variant =
    annotate<defaults_adjacent_annotation>::type<std::variant<defaults_alt_a, defaults_alt_b>>;

KOTATSU_ANNOTATION(defaults_external_annotation, tagged = true, tag_names = {"a", "b"});
using defaults_external_variant =
    annotate<defaults_external_annotation>::type<std::variant<defaults_alt_a, defaults_alt_b>>;

struct defaults_elem_a {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::int32_t> alpha = 7;
};

struct defaults_elem_b {
    KOTATSU_ANNOTATE(defaulted = true)
    <std::string> beta = "cell";
};

struct defaults_elem_c {
    KOTATSU_ANNOTATE(defaulted = true)
    <bool> gamma = true;
};

struct defaults_containers {
    std::vector<defaults_elem_a> pool = {{}};
    std::tuple<defaults_elem_b, std::int32_t> entry;
    std::map<std::string, defaults_elem_c> index = {
        {"main", {}}
    };
};

namespace json = kota::codec::json;

template <typename T>
void check_root_integer_schema() {
    const auto result = json::schema_string<T>().value();
    std::string expected;
    if constexpr(std::is_signed_v<T>) {
        expected = std::format(
            R"({{"$schema":"https://json-schema.org/draft/2020-12/schema","type":"integer","minimum":{},"maximum":{}}})",
            static_cast<std::int64_t>(std::numeric_limits<T>::min()),
            static_cast<std::int64_t>(std::numeric_limits<T>::max()));
    } else {
        expected = std::format(
            R"({{"$schema":"https://json-schema.org/draft/2020-12/schema","type":"integer","minimum":0,"maximum":{}}})",
            static_cast<std::uint64_t>(std::numeric_limits<T>::max()));
    }
    EXPECT_EQ(result, expected);
}

template <typename Wrapper, typename T>
void check_wrapper_integer_schema() {
    const auto result = json::schema_string<Wrapper>().value();
    std::string expected;
    if constexpr(std::is_signed_v<T>) {
        expected = std::format(
            R"({{"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{{"v":{{"type":"integer","minimum":{},"maximum":{}}}}},"required":["v"]}})",
            static_cast<std::int64_t>(std::numeric_limits<T>::min()),
            static_cast<std::int64_t>(std::numeric_limits<T>::max()));
    } else {
        expected = std::format(
            R"({{"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{{"v":{{"type":"integer","minimum":0,"maximum":{}}}}},"required":["v"]}})",
            static_cast<std::uint64_t>(std::numeric_limits<T>::max()));
    }
    EXPECT_EQ(result, expected);
}

TEST_SUITE(serde_json_schema) {

// ---------------------------------------------------------------------------
// Root scalars
// ---------------------------------------------------------------------------

TEST_CASE(root_bool) {
    const auto result = json::schema_string<bool>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"boolean"})");
}

TEST_CASE(root_integers) {
    check_root_integer_schema<std::int8_t>();
    check_root_integer_schema<std::int16_t>();
    check_root_integer_schema<std::int32_t>();
    check_root_integer_schema<std::int64_t>();
    check_root_integer_schema<std::uint8_t>();
    check_root_integer_schema<std::uint16_t>();
    check_root_integer_schema<std::uint32_t>();
    check_root_integer_schema<std::uint64_t>();
}

TEST_CASE(root_floats) {
    // The default nan_repr (Passthrough) hands non-finite values to the
    // writer, which emits null — so even the default schema admits null.
    const auto schema =
        R"({"$schema":"https://json-schema.org/draft/2020-12/schema","anyOf":[{"type":"number"},{"type":"null"}]})";
    EXPECT_EQ(json::schema_string<float>().value(), schema);
    EXPECT_EQ(json::schema_string<double>().value(), schema);
}

TEST_CASE(root_char) {
    const auto result = json::schema_string<char>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"string"})");
}

TEST_CASE(root_string) {
    const auto result = json::schema_string<std::string>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"string"})");
}

// ---------------------------------------------------------------------------
// Scalar struct wrappers
// ---------------------------------------------------------------------------

TEST_CASE(scalar_wrapper_bool) {
    const auto result = json::schema_string<s_bool>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"boolean"}},)"
              R"("required":["v"]})");
}

TEST_CASE(scalar_wrapper_integers) {
    check_wrapper_integer_schema<s_i8, std::int8_t>();
    check_wrapper_integer_schema<s_i16, std::int16_t>();
    check_wrapper_integer_schema<s_i32, std::int32_t>();
    check_wrapper_integer_schema<s_i64, std::int64_t>();
    check_wrapper_integer_schema<s_u8, std::uint8_t>();
    check_wrapper_integer_schema<s_u16, std::uint16_t>();
    check_wrapper_integer_schema<s_u32, std::uint32_t>();
    check_wrapper_integer_schema<s_u64, std::uint64_t>();
}

TEST_CASE(scalar_wrapper_floats) {
    const auto schema =
        R"({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"v":{"anyOf":[{"type":"number"},{"type":"null"}]}},"required":["v"]})";
    EXPECT_EQ(json::schema_string<s_f32>().value(), schema);
    EXPECT_EQ(json::schema_string<s_f64>().value(), schema);
}

TEST_CASE(scalar_wrapper_char) {
    const auto result = json::schema_string<s_char>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"string"}},)"
              R"("required":["v"]})");
}

TEST_CASE(scalar_wrapper_str) {
    const auto result = json::schema_string<s_str>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"string"}},)"
              R"("required":["v"]})");
}

// ---------------------------------------------------------------------------
// Root enums
// ---------------------------------------------------------------------------

TEST_CASE(root_enum_color_i8) {
    const auto result = json::schema_string<color_i8>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"integer","minimum":-128,"maximum":127})");
}

TEST_CASE(root_enum_single) {
    const auto result = json::schema_string<single_enum>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"integer","minimum":-2147483648,"maximum":2147483647})");
}

TEST_CASE(root_enum_status) {
    const auto result = json::schema_string<status>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"integer","minimum":-2147483648,"maximum":2147483647})");
}

TEST_CASE(root_enum_flag_u8) {
    const auto result = json::schema_string<flag_u8>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"integer","minimum":0,"maximum":255})");
}

TEST_CASE(root_enum_level_i16) {
    const auto result = json::schema_string<level_i16>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"integer","minimum":-32768,"maximum":32767})");
}

TEST_CASE(root_enum_value_outside_name_scan) {
    // 65535 lies outside the [-128, 127] name-reflection scan, yet encodes
    // fine as a number — the schema must not reject it, so the numeric form
    // is the underlying integer's range rather than a reflected value list.
    enum class big_u16 : std::uint16_t { a = 0, c = 65535 };
    const auto encoded = json::to_string(big_u16::c);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, "65535");

    const auto result = json::schema_string<big_u16>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"integer","minimum":0,"maximum":65535})");
}

// ---------------------------------------------------------------------------
// Containers
// ---------------------------------------------------------------------------

TEST_CASE(container_vec_i32) {
    const auto result = json::schema_string<s_vec_i32>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}},)"
              R"("required":["v"]})");
}

TEST_CASE(container_set_i32) {
    const auto result = json::schema_string<s_set_i32>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("uniqueItems":true}},)"
              R"("required":["v"]})");
}

TEST_CASE(container_map_str_i32) {
    const auto result = json::schema_string<s_map_str_i32>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"object",)"
              R"("additionalProperties":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}},)"
              R"("required":["v"]})");
}

TEST_CASE(container_vec_vec_i32) {
    const auto result = json::schema_string<s_vec_vec_i32>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"array",)"
              R"("items":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}}},)"
              R"("required":["v"]})");
}

TEST_CASE(map_str_vec_i32) {
    const auto result = json::schema_string<s_map_str_vec_i32>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"object",)"
              R"("additionalProperties":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}}},)"
              R"("required":["v"]})");
}

// ---------------------------------------------------------------------------
// Tuple / Pair
// ---------------------------------------------------------------------------

TEST_CASE(tuple_pair) {
    const auto result = json::schema_string<s_pair>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"array",)"
              R"("prefixItems":[)"
              R"({"type":"string"},)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}],)"
              R"("items":false,)"
              R"("minItems":2,"maxItems":2}},)"
              R"("required":["v"]})");
}

TEST_CASE(tuple_triple) {
    const auto result = json::schema_string<s_tuple>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"array",)"
              R"("prefixItems":[)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"string"},)"
              R"({"type":"boolean"}],)"
              R"("items":false,)"
              R"("minItems":3,"maxItems":3}},)"
              R"("required":["v"]})");
}

TEST_CASE(tuple_pair_in_struct) {
    const auto result = json::schema_string<with_pair_field>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("p":{"type":"array",)"
              R"("prefixItems":[)"
              R"({"type":"string"},)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}],)"
              R"("items":false,)"
              R"("minItems":2,"maxItems":2},)"
              R"("name":{"type":"string"}},)"
              R"("required":["p","name"]})");
}

TEST_CASE(tuple_in_struct) {
    const auto result = json::schema_string<with_tuple_field>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("t":{"type":"array",)"
              R"("prefixItems":[)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"boolean"}],)"
              R"("items":false,)"
              R"("minItems":2,"maxItems":2},)"
              R"("name":{"type":"string"}},)"
              R"("required":["t","name"]})");
}

// ---------------------------------------------------------------------------
// Basic structs
// ---------------------------------------------------------------------------

TEST_CASE(struct_empty) {
    const auto result = json::schema_string<empty_struct>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{}})");
}

TEST_CASE(struct_single_field) {
    const auto result = json::schema_string<single_field>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x"]})");
}

TEST_CASE(struct_point2d) {
    const auto result = json::schema_string<point2d>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]})");
}

TEST_CASE(struct_with_string) {
    const auto result = json::schema_string<with_string>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("value":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["name","value"]})");
}

// ---------------------------------------------------------------------------
// Nested structs
// ---------------------------------------------------------------------------

TEST_CASE(nested_inner) {
    const auto result = json::schema_string<inner>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["a"]})");
}

TEST_CASE(nested_middle) {
    const auto result = json::schema_string<middle>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("i":{"$ref":"#/$defs/inner"},)"
              R"("s":{"type":"string"}},)"
              R"("required":["i","s"],)"
              R"("$defs":{)"
              R"("inner":{"type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["a"]}}})");
}

TEST_CASE(nested_outer) {
    const auto result = json::schema_string<outer>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("m":{"$ref":"#/$defs/middle"},)"
              R"("n":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["m","n"],)"
              R"("$defs":{)"
              R"("inner":{"type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["a"]},)"
              R"("middle":{"type":"object",)"
              R"("properties":{)"
              R"("i":{"$ref":"#/$defs/inner"},)"
              R"("s":{"type":"string"}},)"
              R"("required":["i","s"]}}})");
}

TEST_CASE(nested_with_enum) {
    const auto result = json::schema_string<with_enum>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("c":{"type":"integer","minimum":-128,"maximum":127},)"
              R"("name":{"type":"string"}},)"
              R"("required":["c","name"]})");
}

// ---------------------------------------------------------------------------
// Optional / pointer
// ---------------------------------------------------------------------------

TEST_CASE(optional_field) {
    const auto result = json::schema_string<with_optional>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("age":{"anyOf":[{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"null"}],"default":null}},)"
              R"("required":["name"]})");
}

TEST_CASE(unique_ptr_field) {
    const auto result = json::schema_string<with_unique>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("ptr":{"anyOf":[{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"null"}],"default":null}},)"
              R"("required":["name"]})");
}

TEST_CASE(shared_ptr_field) {
    const auto result = json::schema_string<with_shared>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("ptr":{"anyOf":[{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"null"}],"default":null}},)"
              R"("required":["name"]})");
}

TEST_CASE(all_optional_fields) {
    const auto result = json::schema_string<all_optional>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("a":{"anyOf":[{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"null"}],"default":null},)"
              R"("b":{"anyOf":[{"type":"string"},)"
              R"({"type":"null"}],"default":null}}})");
}

TEST_CASE(all_ptr_types) {
    const auto result = json::schema_string<with_all_ptr>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("opt":{"anyOf":[{"type":"string"},)"
              R"({"type":"null"}],"default":null},)"
              R"("uniq":{"anyOf":[{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"null"}],"default":null},)"
              R"("shr":{"anyOf":[{"type":"boolean"},)"
              R"({"type":"null"}],"default":null}}})");
}

// ---------------------------------------------------------------------------
// default_value attribute
// ---------------------------------------------------------------------------

TEST_CASE(attr_default_value) {
    const auto result = json::schema_string<with_default>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("count":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647,"default":0}},)"
              R"("required":["name"]})");
}

TEST_CASE(all_default_fields) {
    const auto result = json::schema_string<all_default>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647,"default":0},)"
              R"("y":{"type":"string","default":""}}})");
}

// ---------------------------------------------------------------------------
// deny_unknown_fields
// ---------------------------------------------------------------------------

TEST_CASE(deny_unknown_struct) {
    const static field_info deny_fields[] = {
        {"name",  {}, 0, 0, type_info_of<std::string>,  false, false, false},
        {"count", {}, 0, 1, type_info_of<std::int32_t>, false, false, false},
    };
    const static struct_type_info deny_info = {
        {type_kind::structure, "deny_struct"},
        true, // deny_unknown
        false, // is_trivial_layout
        {deny_fields,          2            },
    };
    const auto result = json::schema_string(deny_info).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("count":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["name","count"],)"
              R"("additionalProperties":false})");
}

// ---------------------------------------------------------------------------
// skip
// ---------------------------------------------------------------------------

TEST_CASE(attr_skip) {
    const auto result = json::schema_string<with_skip>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("visible":{"type":"string"}},)"
              R"("required":["visible"]})");
}

TEST_CASE(skip_and_default) {
    const auto result = json::schema_string<skip_default>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("count":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647,"default":0}},)"
              R"("required":["name"]})");
}

// A field the encoder may omit (built-in skip_when or a custom skip_if
// predicate) is never required — the decoder accepts its absence.
TEST_CASE(skip_if_fields_not_required) {
    const auto result = json::schema_string<with_skip_when>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("tags":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("count":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("note":{"type":"string"}},)"
              R"("required":["name"]})");
}

// Two KOTATSU_ANNOTATE uses expand to distinct tags; identical untagged
// struct specs must still collapse to one type_info instance and $defs entry.
TEST_CASE(repeated_inline_struct_annotation_shares_def) {
    const auto result = json::schema_string<repeated_child_annotation>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("left":{"$ref":"#/$defs/casing_child"},)"
              R"("right":{"$ref":"#/$defs/casing_child"}},)"
              R"("required":["left","right"],)"
              R"("$defs":{)"
              R"("casing_child":{"type":"object",)"
              R"("properties":{)"
              R"("firstValue":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["firstValue"]}}})");
}

// ---------------------------------------------------------------------------
// flatten
// ---------------------------------------------------------------------------

TEST_CASE(attr_flatten) {
    const auto result = json::schema_string<with_flatten>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("b":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("extra":{"type":"string"}},)"
              R"("required":["a","b","extra"]})");
}

TEST_CASE(flatten_with_optional) {
    const auto result = json::schema_string<flatten_opt>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"anyOf":[{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"null"}],"default":null},)"
              R"("tag":{"type":"string"}},)"
              R"("required":["x","tag"]})");
}

TEST_CASE(flatten_with_rename) {
    const auto result = json::schema_string<flatten_rename>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("alpha":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("b":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("extra":{"type":"string"}},)"
              R"("required":["alpha","b","extra"]})");
}

// ---------------------------------------------------------------------------
// rename
// ---------------------------------------------------------------------------

TEST_CASE(attr_rename) {
    const auto result = json::schema_string<with_rename>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("my_field":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"string"}},)"
              R"("required":["my_field","y"]})");
}

// ---------------------------------------------------------------------------
// Variant (tag_mode::none)
// ---------------------------------------------------------------------------

TEST_CASE(variant_untagged) {
    const auto result = json::schema_string<var_none>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"anyOf":[)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"string"}]}},)"
              R"("required":["v"]})");
}

TEST_CASE(variant_three_alts) {
    const auto result = json::schema_string<var_three>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"anyOf":[)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"string"},)"
              R"({"type":"boolean"}]}},)"
              R"("required":["v"]})");
}

// ---------------------------------------------------------------------------
// Variant (tag_mode::external)
// ---------------------------------------------------------------------------

TEST_CASE(variant_external_tag) {
    const static type_info_fn ext_alts[] = {
        type_info_of<std::int32_t>,
        type_info_of<std::string>,
    };
    const static std::string_view ext_names[] = {"num", "text"};
    const static variant_type_info ext_var = {
        {type_kind::variant, "ext_var"},
        {ext_alts, 2},
        tag_mode::external,
        {},
        {},
        {ext_names, 2},
    };
    const static type_info_fn ext_var_ref = []() -> const type_info& {
        return ext_var;
    };
    const static field_info ext_field = {
        "v",
        {},
        0,
        0,
        ext_var_ref,
        false,
        false,
        false,
    };
    const static struct_type_info ext_wrap = {
        {type_kind::structure, "ext_wrap"},
        false,
        false,
        {&ext_field,           1         },
    };
    const auto result = json::schema_string(ext_wrap).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"oneOf":[)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("num":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["num"],)"
              R"("additionalProperties":false},)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("text":{"type":"string"}},)"
              R"("required":["text"],)"
              R"("additionalProperties":false}]}},)"
              R"("required":["v"]})");
}

// ---------------------------------------------------------------------------
// Variant (tag_mode::internal)
// ---------------------------------------------------------------------------

TEST_CASE(variant_internal_tag) {
    const static type_info_fn int_alts[] = {
        type_info_of<point2d>,
        type_info_of<inner>,
    };
    const static std::string_view int_names[] = {"point", "inner"};
    const static variant_type_info int_var = {
        {type_kind::variant, "int_var"},
        {int_alts, 2},
        tag_mode::internal,
        "type",
        {},
        {int_names, 2},
    };
    const static type_info_fn int_var_ref = []() -> const type_info& {
        return int_var;
    };
    const static field_info int_field = {
        "v",
        {},
        0,
        0,
        int_var_ref,
        false,
        false,
        false,
    };
    const static struct_type_info int_wrap = {
        {type_kind::structure, "int_wrap"},
        false,
        false,
        {&int_field,           1         },
    };
    const auto result = json::schema_string(int_wrap).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"oneOf":[)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"("y":{"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"("type":{"const":"point"}},)"
              R"("required":["x","y","type"]},)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"("type":{"const":"inner"}},)"
              R"("required":["a","type"]}]}},)"
              R"("required":["v"]})");
}

// ---------------------------------------------------------------------------
// Variant (tag_mode::adjacent)
// ---------------------------------------------------------------------------

TEST_CASE(variant_adjacent_tag) {
    const static type_info_fn adj_alts[] = {
        type_info_of<std::int32_t>,
        type_info_of<std::string>,
    };
    const static std::string_view adj_names[] = {"num", "text"};
    const static variant_type_info adj_var = {
        {type_kind::variant, "adj_var"},
        {adj_alts,           2        },
        tag_mode::adjacent,
        "t",
        "c",
        {adj_names,          2        },
    };
    const static type_info_fn adj_var_ref = []() -> const type_info& {
        return adj_var;
    };
    const static field_info adj_field = {
        "v",
        {},
        0,
        0,
        adj_var_ref,
        false,
        false,
        false,
    };
    const static struct_type_info adj_wrap = {
        {type_kind::structure, "adj_wrap"},
        false,
        false,
        {&adj_field,           1         },
    };
    const auto result = json::schema_string(adj_wrap).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"oneOf":[)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("t":{"const":"num"},)"
              R"("c":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["t","c"],)"
              R"("additionalProperties":false},)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("t":{"const":"text"},)"
              R"("c":{"type":"string"}},)"
              R"("required":["t","c"],)"
              R"("additionalProperties":false}]}},)"
              R"("required":["v"]})");
}

TEST_CASE(root_external_variant) {
    const auto result = json::schema_string<root_external_variant>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("oneOf":[)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("integer":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["integer"],)"
              R"("additionalProperties":false},)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("text":{"type":"string"}},)"
              R"("required":["text"],)"
              R"("additionalProperties":false}]})");
}

TEST_CASE(root_internal_variant) {
    const auto result = json::schema_string<root_internal_variant>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("oneOf":[)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("radius":{"anyOf":[{"type":"number"},{"type":"null"}]},)"
              R"("kind":{"const":"circle"}},)"
              R"("required":["radius","kind"]},)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("width":{"anyOf":[{"type":"number"},{"type":"null"}]},)"
              R"("height":{"anyOf":[{"type":"number"},{"type":"null"}]},)"
              R"("kind":{"const":"rect"}},)"
              R"("required":["width","height","kind"]}]})");
}

TEST_CASE(root_adjacent_variant) {
    const auto result = json::schema_string<root_adjacent_variant>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("oneOf":[)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("type":{"const":"integer"},)"
              R"("value":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["type","value"],)"
              R"("additionalProperties":false},)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("type":{"const":"text"},)"
              R"("value":{"type":"string"}},)"
              R"("required":["type","value"],)"
              R"("additionalProperties":false}]})");
}

TEST_CASE(opaque_root_returns_error) {
    const auto result = json::schema_string<json_schema_opaque_root>();
    EXPECT_TRUE(!result.has_value());
}

TEST_CASE(any_type_root) {
    const static type_info any_ti = {type_kind::any, "any"};
    const auto result = json::schema_string(any_ti).value();
    EXPECT_EQ(result, R"({"$schema":"https://json-schema.org/draft/2020-12/schema"})");
}

TEST_CASE(any_type_field) {
    const static type_info any_ti = {type_kind::any, "any"};
    const static field_info any_fields[] = {
        {"data", {}, 0, 0, []() -> const type_info& { return any_ti; }, false, false, false},
    };
    const static struct_type_info any_struct = {
        {type_kind::structure, "with_any"},
        false,
        false,
        {any_fields,           1         },
    };
    const auto result = json::schema_string(any_struct).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{"data":{}},)"
              R"("required":["data"]})");
}

// ---------------------------------------------------------------------------
// More containers
// ---------------------------------------------------------------------------

TEST_CASE(map_str_struct) {
    const auto result = json::schema_string<map_str_struct>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("entries":{"type":"object",)"
              R"("additionalProperties":{)"
              R"("$ref":"#/$defs/point2d"}}},)"
              R"("required":["entries"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

TEST_CASE(map_str_enum) {
    const auto result = json::schema_string<map_str_enum>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("entries":{"type":"object",)"
              R"("additionalProperties":{)"
              R"("type":"integer","minimum":-128,"maximum":127}}},)"
              R"("required":["entries"]})");
}

TEST_CASE(vec_optional_items) {
    const auto result = json::schema_string<vec_optional>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"type":"array",)"
              R"("items":{"anyOf":[{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"null"}]}}},)"
              R"("required":["v"]})");
}

TEST_CASE(optional_vec_field) {
    const auto result = json::schema_string<optional_vec>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"anyOf":[{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"({"type":"null"}],"default":null}}})");
}

TEST_CASE(vec_of_enum) {
    const auto result = json::schema_string<vec_enum>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("colors":{"type":"array",)"
              R"("items":{)"
              R"("type":"integer","minimum":-128,"maximum":127}}},)"
              R"("required":["colors"]})");
}

TEST_CASE(set_of_string) {
    const auto result = json::schema_string<set_string>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("tags":{"type":"array",)"
              R"("items":{"type":"string"},)"
              R"("uniqueItems":true}},)"
              R"("required":["tags"]})");
}

TEST_CASE(vec_of_map) {
    const auto result = json::schema_string<vec_map>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("items":{"type":"array",)"
              R"("items":{"type":"object",)"
              R"("additionalProperties":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}}},)"
              R"("required":["items"]})");
}

TEST_CASE(map_of_vec_struct) {
    const auto result = json::schema_string<map_vec_struct>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("groups":{"type":"object",)"
              R"("additionalProperties":{"type":"array",)"
              R"("items":{)"
              R"("$ref":"#/$defs/point2d"}}}},)"
              R"("required":["groups"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

TEST_CASE(deep_container_field) {
    const auto result = json::schema_string<deep_container>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("data":{"type":"object",)"
              R"("additionalProperties":{"type":"array",)"
              R"("items":{"type":"object",)"
              R"("additionalProperties":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}}}},)"
              R"("required":["data"]})");
}

TEST_CASE(map_of_map_field) {
    const auto result = json::schema_string<map_of_map>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("m":{"type":"object",)"
              R"("additionalProperties":{"type":"object",)"
              R"("additionalProperties":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}}},)"
              R"("required":["m"]})");
}

// ---------------------------------------------------------------------------
// Struct with pointer to struct
// ---------------------------------------------------------------------------

TEST_CASE(shared_ptr_to_struct) {
    const auto result = json::schema_string<shared_struct>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("point":{"anyOf":[{)"
              R"("$ref":"#/$defs/point2d"},)"
              R"({"type":"null"}],"default":null}},)"
              R"("required":["name"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

TEST_CASE(optional_struct_field) {
    const auto result = json::schema_string<optional_struct>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("point":{"anyOf":[{)"
              R"("$ref":"#/$defs/point2d"},)"
              R"({"type":"null"}],"default":null},)"
              R"("name":{"type":"string"}},)"
              R"("required":["name"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

// ---------------------------------------------------------------------------
// $defs dedup
// ---------------------------------------------------------------------------

TEST_CASE(defs_dedup_multi_ref) {
    const auto result = json::schema_string<multi_ref>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("a":{)"
              R"("$ref":"#/$defs/point2d"},)"
              R"("b":{)"
              R"("$ref":"#/$defs/point2d"},)"
              R"("list":{"type":"array",)"
              R"("items":{)"
              R"("$ref":"#/$defs/point2d"}}},)"
              R"("required":["a","b","list"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

// ---------------------------------------------------------------------------
// Struct with enum fields
// ---------------------------------------------------------------------------

TEST_CASE(multi_enum_fields) {
    const auto result = json::schema_string<multi_enum>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("c":{"type":"integer","minimum":-128,"maximum":127},)"
              R"("s":{"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"("label":{"type":"string"}},)"
              R"("required":["c","s","label"]})");
}

TEST_CASE(with_flag_enum) {
    const auto result = json::schema_string<with_flag>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("f":{"type":"integer","minimum":0,"maximum":255},)"
              R"("name":{"type":"string"}},)"
              R"("required":["f","name"]})");
}

TEST_CASE(with_level_enum) {
    const auto result = json::schema_string<with_level>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("l":{"type":"integer","minimum":-32768,"maximum":32767},)"
              R"("v":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["l","v"]})");
}

// ---------------------------------------------------------------------------
// Nested struct with optional
// ---------------------------------------------------------------------------

TEST_CASE(optional_inner_field) {
    const auto result = json::schema_string<optional_inner>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("i":{"anyOf":[{)"
              R"("$ref":"#/$defs/inner"},)"
              R"({"type":"null"}],"default":null},)"
              R"("name":{"type":"string"}},)"
              R"("required":["name"],)"
              R"("$defs":{)"
              R"("inner":{"type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["a"]}}})");
}

// ---------------------------------------------------------------------------
// Variant in container
// ---------------------------------------------------------------------------

TEST_CASE(vec_of_variant) {
    const auto result = json::schema_string<vec_variant>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("items":{"type":"array",)"
              R"("items":{"anyOf":[)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"string"}]}}},)"
              R"("required":["items"]})");
}

// ---------------------------------------------------------------------------
// Combinations
// ---------------------------------------------------------------------------

TEST_CASE(combo_mixed_fields) {
    const auto result = json::schema_string<combo>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("color":{)"
              R"("type":"integer","minimum":-128,"maximum":127},)"
              R"("label":{"anyOf":[{"type":"string"},)"
              R"({"type":"null"}],"default":null},)"
              R"("values":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("attrs":{"type":"object",)"
              R"("additionalProperties":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}}},)"
              R"("required":["color","values","attrs"]})");
}

TEST_CASE(combo_nested_struct_refs) {
    const auto result = json::schema_string<nested_combo>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("point":{)"
              R"("$ref":"#/$defs/point2d"},)"
              R"("color":{)"
              R"("type":"integer","minimum":-128,"maximum":127},)"
              R"("points":{"type":"array",)"
              R"("items":{)"
              R"("$ref":"#/$defs/point2d"}},)"
              R"("named_points":{"type":"object",)"
              R"("additionalProperties":{)"
              R"("$ref":"#/$defs/point2d"}}},)"
              R"("required":[)"
              R"("point","color","points","named_points"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

TEST_CASE(combo_vec_of_struct) {
    const auto result = json::schema_string<vec_of_struct>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("items":{"type":"array",)"
              R"("items":{)"
              R"("$ref":"#/$defs/point2d"}}},)"
              R"("required":["items"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

TEST_CASE(combo_deep_nesting) {
    const auto result = json::schema_string<deep_outer>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("dm":{)"
              R"("$ref":"#/$defs/deep_middle"},)"
              R"("n":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["dm","n"],)"
              R"("$defs":{)"
              R"("deep_inner":{"type":"object",)"
              R"("properties":{)"
              R"("c":{)"
              R"("type":"integer","minimum":-128,"maximum":127},)"
              R"("v":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["c","v"]},)"
              R"("deep_middle":{"type":"object",)"
              R"("properties":{)"
              R"("di":{)"
              R"("$ref":"#/$defs/deep_inner"},)"
              R"("s":{"type":"string"}},)"
              R"("required":["di","s"]}}})");
}

TEST_CASE(combo_multi_map) {
    const auto result = json::schema_string<multi_map>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"object",)"
              R"("additionalProperties":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("b":{"type":"object",)"
              R"("additionalProperties":{"type":"string"}}},)"
              R"("required":["a","b"]})");
}

TEST_CASE(combo_many_fields) {
    const auto result = json::schema_string<many_fields>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("a":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("b":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("c":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("d":{"type":"string"},)"
              R"("e":{"type":"boolean"},)"
              R"("f":{"anyOf":[{"type":"number"},{"type":"null"}]}},)"
              R"("required":["a","b","c","d","e","f"]})");
}

TEST_CASE(combo_set_of_struct) {
    const auto result = json::schema_string<set_of_struct>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("ids":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("uniqueItems":true},)"
              R"("name":{"type":"string"}},)"
              R"("required":["ids","name"]})");
}

TEST_CASE(combo_trivial_nested) {
    const auto result = json::schema_string<trivial_nested>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("p":{)"
              R"("$ref":"#/$defs/point2d"},)"
              R"("z":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["p","z"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

// ---------------------------------------------------------------------------
// Self-referential struct
// ---------------------------------------------------------------------------

TEST_CASE(self_referential_struct) {
    static struct_type_info self_info = {
        {type_kind::structure, "self_ref"},
        false,
        false,
        {},
    };
    const static optional_type_info opt_self = {
        {type_kind::optional, "optional<self_ref>"},
        []() -> const type_info& { return self_info; },
    };
    const static field_info self_fields[] = {
        {"value", {}, 0, 0, type_info_of<std::int32_t>, false, false, false},
        {"next",
         {},
         0, 1,
         []() -> const type_info& { return opt_self; },
         false, false,
         false, true},
    };
    self_info.fields = {self_fields, 2};

    const auto result = json::schema_string(self_info).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("value":{"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"("next":{"anyOf":[{"$ref":"#"},{"type":"null"}]}},)"
              R"("required":["value"]})");
}

// ---------------------------------------------------------------------------
// Empty enum
// ---------------------------------------------------------------------------

TEST_CASE(empty_enum) {
    const static enum_type_info empty_ei = {
        {type_kind::enumeration, "empty_enum"},
        {},
        nullptr,
        type_kind::int32,
    };
    const auto result = json::schema_string(empty_ei).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"integer","minimum":-2147483648,"maximum":2147483647})");
}

// ---------------------------------------------------------------------------
// Variant nesting variant
// ---------------------------------------------------------------------------

TEST_CASE(variant_of_variant) {
    const static type_info_fn inner_alts[] = {
        type_info_of<std::string>,
        type_info_of<bool>,
    };
    const static variant_type_info inner_var = {
        {type_kind::variant, "inner_variant"},
        {inner_alts, 2},
        tag_mode::none,
        {},
        {},
        {},
    };

    const static type_info_fn outer_alts[] = {
        type_info_of<std::int32_t>,
        []() -> const type_info& { return inner_var; },
    };
    const static variant_type_info outer_var = {
        {type_kind::variant, "outer_variant"},
        {outer_alts, 2},
        tag_mode::none,
        {},
        {},
        {},
    };

    const auto result = json::schema_string(outer_var).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("anyOf":[)"
              R"({"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"({"anyOf":[)"
              R"({"type":"string"},)"
              R"({"type":"boolean"}]}]})");
}

// ---------------------------------------------------------------------------
// Field ordering stability
// ---------------------------------------------------------------------------

TEST_CASE(field_ordering_stability) {
    const static field_info ordered_fields[] = {
        {"zebra",  {}, 0, 0, type_info_of<std::string>,  false, false, false},
        {"alpha",  {}, 0, 1, type_info_of<std::int32_t>, false, false, false},
        {"middle", {}, 0, 2, type_info_of<bool>,         false, false, false},
        {"beta",   {}, 0, 3, type_info_of<double>,       false, false, false},
    };
    const static struct_type_info ordered_info = {
        {type_kind::structure, "ordered_struct"},
        false,
        false,
        {ordered_fields,       4               },
    };
    const auto result = json::schema_string(ordered_info).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("zebra":{"type":"string"},)"
              R"("alpha":{"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"("middle":{"type":"boolean"},)"
              R"("beta":{"anyOf":[{"type":"number"},{"type":"null"}]}},)"
              R"("required":["zebra","alpha","middle","beta"]})");
}

// ---------------------------------------------------------------------------
// Variant with monostate
// ---------------------------------------------------------------------------

struct with_monostate {
    std::variant<std::monostate, std::int32_t, std::string> v;
};

TEST_CASE(variant_with_monostate) {
    const auto result = json::schema_string<with_monostate>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("v":{"anyOf":[)"
              R"({"type":"null"},)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"string"}]}},)"
              R"("required":["v"]})");
}

// ---------------------------------------------------------------------------
// Mutual recursion
// ---------------------------------------------------------------------------

TEST_CASE(mutual_recursion) {
    static struct_type_info info_a = {
        {type_kind::structure, "node_a"},
        false,
        false,
        {}
    };
    static struct_type_info info_b = {
        {type_kind::structure, "node_b"},
        false,
        false,
        {}
    };

    const static optional_type_info opt_b = {
        {type_kind::optional, "optional<node_b>"},
        []() -> const type_info& { return info_b; },
    };
    const static optional_type_info opt_a = {
        {type_kind::optional, "optional<node_a>"},
        []() -> const type_info& { return info_a; },
    };

    const static field_info fields_a[] = {
        {"value", {}, 0, 0, type_info_of<std::int32_t>, false, false, false},
        {"b", {}, 0, 1, []() -> const type_info& { return opt_b; }, false, false, false, true},
    };
    const static field_info fields_b[] = {
        {"name", {}, 0, 0, type_info_of<std::string>, false, false, false},
        {"a", {}, 0, 1, []() -> const type_info& { return opt_a; }, false, false, false, true},
    };
    info_a.fields = {fields_a, 2};
    info_b.fields = {fields_b, 2};

    const auto result = json::schema_string(info_a).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("value":{"type":"integer","minimum":-2147483648,"maximum":2147483647},)"
              R"("b":{"anyOf":[{"$ref":"#/$defs/node_b"},{"type":"null"}]}},)"
              R"("required":["value"],)"
              R"("$defs":{)"
              R"("node_b":{"type":"object",)"
              R"("properties":{)"
              R"("name":{"type":"string"},)"
              R"("a":{"anyOf":[{"$ref":"#"},{"type":"null"}]}},)"
              R"("required":["name"]}}})");
}

// ---------------------------------------------------------------------------
// Bytes type
// ---------------------------------------------------------------------------

TEST_CASE(bytes_field) {
    const static type_info bytes_ti = {type_kind::bytes, "bytes"};
    const static field_info bytes_fields[] = {
        {"data", {}, 0, 0, []() -> const type_info& { return bytes_ti; }, false, false, false},
    };
    const static struct_type_info bytes_struct = {
        {type_kind::structure, "with_bytes"},
        false,
        false,
        {bytes_fields,         1           },
    };
    const auto result = json::schema_string(bytes_struct).value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("data":{"type":"array",)"
              R"("items":{"type":"integer",)"
              R"("minimum":0,)"
              R"("maximum":255}}},)"
              R"("required":["data"]})");
}

// ---------------------------------------------------------------------------
// description
// ---------------------------------------------------------------------------

TEST_CASE(description_on_scalar_field) {
    const auto result = json::schema_string<desc_scalar>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("threads":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647,)"
              R"("description":"Number of worker threads."},)"
              R"("name":{"type":"string"}},)"
              R"("required":["threads","name"]})");
}

TEST_CASE(description_on_optional_field) {
    const auto result = json::schema_string<desc_optional>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("label":{"anyOf":[{"type":"string"},)"
              R"({"type":"null"}],)"
              R"("description":"Optional display label.","default":null}}})");
}

TEST_CASE(description_on_struct_ref_field) {
    const auto result = json::schema_string<desc_struct_ref>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("anchor":{"$ref":"#/$defs/point2d",)"
              R"("description":"Anchor position."}},)"
              R"("required":["anchor"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

TEST_CASE(description_through_flatten) {
    const auto result = json::schema_string<desc_flatten>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("count":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647,)"
              R"("description":"Inherited counter."},)"
              R"("tag":{"type":"string"}},)"
              R"("required":["count","tag"]})");
}

TEST_CASE(description_with_rename) {
    const auto result = json::schema_string<desc_rename>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("max_size":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647,)"
              R"("description":"Maximum size in bytes."}},)"
              R"("required":["max_size"]})");
}

TEST_CASE(description_with_default_value) {
    const auto result = json::schema_string<desc_default>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("retries":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647,)"
              R"("description":"Retry limit.","default":0}}})");
}

TEST_CASE(described_and_bare_struct_share_def) {
    const auto result = json::schema_string<desc_shared_ref>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("origin":{"$ref":"#/$defs/point2d"},)"
              R"("anchor":{"$ref":"#/$defs/point2d",)"
              R"("description":"Anchor position."}},)"
              R"("required":["origin","anchor"],)"
              R"("$defs":{)"
              R"("point2d":{"type":"object",)"
              R"("properties":{)"
              R"("x":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"("y":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["x","y"]}}})");
}

TEST_CASE(description_in_internal_tagged_alternative) {
    const auto result = json::schema_string<desc_internal_variant>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("oneOf":[)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("radius":{"anyOf":[{"type":"number"},{"type":"null"}],)"
              R"("description":"Radius in meters."},)"
              R"("kind":{"const":"circle"}},)"
              R"("required":["radius","kind"]},)"
              R"({"type":"object",)"
              R"("properties":{)"
              R"("width":{"anyOf":[{"type":"number"},{"type":"null"}]},)"
              R"("height":{"anyOf":[{"type":"number"},{"type":"null"}]},)"
              R"("kind":{"const":"rect"}},)"
              R"("required":["width","height","kind"]}]})");
}

// ---------------------------------------------------------------------------
// enum representation config
// ---------------------------------------------------------------------------

struct string_enum_config {
    [[maybe_unused]] constexpr static auto enum_repr = codec::enum_repr::String;
};

TEST_CASE(enum_names_under_string_config) {
    const auto result = json::schema_string<color_i8, string_enum_config>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("enum":["red","green","blue"]})");
}

TEST_CASE(unnamed_enum_value_rejected_under_string_config) {
    // A representable value without a reflected member name has no string
    // spelling: the encoder rejects it, so the schema's enum list of
    // reflected names stays exhaustive.
    const auto encoded = json::to_string<string_enum_config>(static_cast<color_i8>(42));
    EXPECT_FALSE(encoded.has_value());
}

TEST_CASE(schema_agrees_with_encoder_on_enums) {
    // Under the default config the encoder emits the numeric value, so the
    // schema constrains the same numeric form.
    const auto encoded = json::to_string(with_enum{.c = color_i8::green, .name = "g"});
    ASSERT_TRUE(encoded.has_value());
    EXPECT_TRUE(encoded->find(R"("c":1)") != std::string::npos);

    const auto schema = json::schema_string<with_enum>().value();
    EXPECT_TRUE(schema.find(R"("c":{"type":"integer","minimum":-128,"maximum":127})") !=
                std::string::npos);
}

struct renamed_enum_config {
    [[maybe_unused]] constexpr static auto enum_repr = codec::enum_repr::String;
    using enum_rename = naming::rename_policy::upper_snake;
};

TEST_CASE(schema_agrees_with_encoder_on_enum_rename) {
    // The schema lists the spellings the encoder writes under the same
    // config, not the raw reflected names.
    const auto encoded = json::to_string<renamed_enum_config>(color_i8::green);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("GREEN")");

    const auto result = json::schema_string<color_i8, renamed_enum_config>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("enum":["RED","GREEN","BLUE"]})");
}

// ---------------------------------------------------------------------------
// nan_repr config
// ---------------------------------------------------------------------------

struct nan_null_config {
    [[maybe_unused]] constexpr static auto nan_repr = codec::nan_repr::Null;
};

struct nan_string_config {
    [[maybe_unused]] constexpr static auto nan_repr = codec::nan_repr::String;
};

TEST_CASE(schema_agrees_with_encoder_on_nan_passthrough) {
    // The default Passthrough forwards the non-finite value to the writer,
    // whose only JSON spelling for it is null — the schema must admit that.
    const auto encoded = json::to_string(std::numeric_limits<double>::quiet_NaN());
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, "null");

    const auto result = json::schema_string<double>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("anyOf":[{"type":"number"},{"type":"null"}]})");
}

TEST_CASE(schema_agrees_with_encoder_on_nan_null) {
    const auto encoded = json::to_string<nan_null_config>(std::numeric_limits<double>::quiet_NaN());
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, "null");

    const auto result = json::schema_string<double, nan_null_config>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("anyOf":[{"type":"number"},{"type":"null"}]})");
}

TEST_CASE(schema_agrees_with_encoder_on_nan_string) {
    const auto encoded = json::to_string<nan_string_config>(std::numeric_limits<float>::infinity());
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("Infinity")");

    const auto result = json::schema_string<float, nan_string_config>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("anyOf":[{"type":"number"},)"
              R"({"enum":["NaN","Infinity","-Infinity"]}]})");
}

struct nan_error_config {
    [[maybe_unused]] constexpr static auto nan_repr = codec::nan_repr::Error;
};

TEST_CASE(schema_agrees_with_encoder_on_long_double_overflow) {
    // A finite long double beyond double's range narrows to infinity in the
    // document, so the nan_repr policy judges the narrowed value: String
    // spells it, Error rejects it — never a null the schema does not admit.
    constexpr long double big = std::numeric_limits<long double>::max();
    if constexpr(big > static_cast<long double>(std::numeric_limits<double>::max())) {
        const auto spelled = json::to_string<nan_string_config>(big);
        ASSERT_TRUE(spelled.has_value());
        EXPECT_EQ(*spelled, R"("Infinity")");

        const auto negative = json::to_string<nan_string_config>(-big);
        ASSERT_TRUE(negative.has_value());
        EXPECT_EQ(*negative, R"("-Infinity")");

        EXPECT_FALSE(json::to_string<nan_error_config>(big).has_value());
    } else {
        // long double is double: the value stays a finite number.
        EXPECT_TRUE(json::to_string<nan_error_config>(big).has_value());
    }
}

// ---------------------------------------------------------------------------
// human_readable config
// ---------------------------------------------------------------------------

struct non_hr_config {
    [[maybe_unused]] constexpr static bool human_readable = false;
};

TEST_CASE(schema_agrees_with_encoder_on_non_human_readable) {
    // A non-human-readable config bypasses tagging and encodes the underlying
    // variant, so the schema describes the untagged alternatives.
    const auto encoded = json::to_string<non_hr_config>(root_external_variant{7});
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, "7");

    const auto result = json::schema_string<root_external_variant, non_hr_config>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("anyOf":[)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647},)"
              R"({"type":"string"}]})");
}

// ---------------------------------------------------------------------------
// overlapping untagged alternatives
// ---------------------------------------------------------------------------

TEST_CASE(untagged_overlap_validates_as_any_of) {
    // A numeric enum's underlying range overlaps the int alternative: both
    // branches match the same document, so exactly-one (oneOf) semantics
    // would reject every value the encoder emits — anyOf must apply.
    using overlapping = std::variant<color_i8, std::int32_t>;
    const auto result = json::schema_string<overlapping>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("anyOf":[)"
              R"({"type":"integer","minimum":-128,"maximum":127},)"
              R"({"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}]})");
}

// ---------------------------------------------------------------------------
// metadata config forwarding
// ---------------------------------------------------------------------------

struct camel_deny_config {
    using field_rename = naming::rename_policy::lower_camel;
    [[maybe_unused]] constexpr static bool deny_unknown_fields = true;
};

TEST_CASE(schema_agrees_with_encoder_on_config) {
    // The schema must accept what to_string under the same config emits:
    // renamed field names and the unknown-field policy.
    const auto encoded = json::to_string<camel_deny_config>(casing_child{.first_value = 7});
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"firstValue":7})");

    const auto result = json::schema_string<casing_child, camel_deny_config>().value();
    EXPECT_EQ(result,
              R"({"$schema":"https://json-schema.org/draft/2020-12/schema",)"
              R"("type":"object",)"
              R"("properties":{)"
              R"("firstValue":{"type":"integer",)"
              R"("minimum":-2147483648,)"
              R"("maximum":2147483647}},)"
              R"("required":["firstValue"],)"
              R"("additionalProperties":false})");
}

// ---------------------------------------------------------------------------
// default annotations from a default-constructed instance
// ---------------------------------------------------------------------------

TEST_CASE(defaults_annotated) {
    const auto result = json::schema_string<defaults_root>().value();
    // Non-required root fields carry the value a default-constructed
    // instance encodes.
    EXPECT_TRUE(result.find(R"("enabled":{"type":"boolean","default":true})") != std::string::npos);
    EXPECT_TRUE(result.find(R"({"type":"null"}],"default":null})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default":[]})") != std::string::npos);
    // The shared defaults_leaf $def is annotated once, inside $defs; the ref
    // sites stay bare.
    EXPECT_TRUE(result.find(R"("pool":{"$ref":"#/$defs/defaults_leaf"})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("mirror":{"$ref":"#/$defs/defaults_leaf"})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":4})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("name":{"type":"string","default":"worker"})") != std::string::npos);
}

TEST_CASE(defaults_skip_condition) {
    // The empty vector triggers skip_if at encode time, so the default
    // document has no such property to annotate from.
    const auto result = json::schema_string<defaults_skipped>().value();
    EXPECT_TRUE(result.find(R"("tags")") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default")") == std::string::npos);
}

TEST_CASE(defaults_shared_def_first_visit_wins) {
    // The two sites default-construct differently (mirror overrides threads
    // via its member initializer), but both are required — no site default —
    // and the shared $def keeps the values of the first visit; the override
    // appears nowhere.
    const auto result = json::schema_string<defaults_shared_override>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":4})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default":9)") == std::string::npos);
}

TEST_CASE(defaults_non_required_ref_sites) {
    // Non-required refs to a shared $def each carry their whole encoded
    // object as the property default, so the per-site member initializer
    // survives even though the $def keeps first-visit leaf values.
    const auto result = json::schema_string<defaults_ref_sites>().value();
    EXPECT_TRUE(
        result.find(
            R"("pool":{"$ref":"#/$defs/defaults_leaf","default":{"threads":4,"name":"worker"}})") !=
        std::string::npos);
    EXPECT_TRUE(
        result.find(
            R"("mirror":{"$ref":"#/$defs/defaults_leaf","default":{"threads":9,"name":"worker"}})") !=
        std::string::npos);
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":4})") != std::string::npos);
}

TEST_CASE(defaults_nullable_root) {
    // A nullable root unwraps to the struct body, but its default instance
    // encodes to null — a document with no properties to annotate from, not
    // a crash.
    const auto opt = json::schema_string<std::optional<defaults_leaf>>().value();
    EXPECT_TRUE(opt.find(R"("threads")") != std::string::npos);
    EXPECT_TRUE(opt.find(R"("default")") == std::string::npos);

    const auto ptr = json::schema_string<std::unique_ptr<defaults_leaf>>().value();
    EXPECT_TRUE(ptr.find(R"("default")") == std::string::npos);
}

TEST_CASE(defaults_engaged_optional) {
    // An engaged optional lands its whole encoded value as the default on
    // the anyOf wrapper; the walk does not descend through anyOf, so the
    // struct's $def stays annotation-free.
    const auto result = json::schema_string<defaults_engaged>().value();
    EXPECT_TRUE(result.find(R"("default":5)") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default":{"threads":4,"name":"worker"})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("name":{"type":"string"}})") != std::string::npos);
}

TEST_CASE(defaults_recursive_root) {
    // A self-referential root terminates: the pointer self-reference sits
    // inside an anyOf wrapper, which is a leaf for the walk, and carries the
    // disengaged pointer's null.
    const auto result = json::schema_string<defaults_node>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":1})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("next":{"anyOf":[{"$ref":"#"},{"type":"null"}],"default":null})") !=
                std::string::npos);
}

TEST_CASE(defaults_internal_tagged_variant_member) {
    // A required tagged-variant property is no leaf: the walk follows the
    // oneOf branch whose tag constraint the default document satisfies, so
    // the encoded alternative's defaulted member keeps its initializer while
    // the unselected branch stays bare.
    const auto result = json::schema_string<defaults_variant_holder>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":3})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default":9)") == std::string::npos);
}

TEST_CASE(defaults_tagged_variant_root) {
    // The same walk applies to a tagged variant at the root, whose oneOf is
    // merged into the top-level schema object.
    const auto result = json::schema_string<defaults_internal_variant>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":3})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default":9)") == std::string::npos);
}

TEST_CASE(defaults_adjacent_tagged_variant) {
    // Adjacent tagging routes the alternative behind the content property, a
    // struct $ref: the matched branch recurses into its $def, while the
    // unselected alternative's $def stays annotation-free.
    const auto result = json::schema_string<defaults_adjacent_variant>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":3})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default":9)") == std::string::npos);
}

TEST_CASE(defaults_external_tagged_variant) {
    // External tagging carries no tag const; the branch requiring the
    // encoded alternative's name matches, and the $ref under it recurses.
    const auto result = json::schema_string<defaults_external_variant>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":3})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("default":9)") == std::string::npos);
}

TEST_CASE(defaults_container_elements) {
    // Required containers carry no site default, but the walk descends
    // alongside their encoded elements — each tuple slot against its prefix
    // schema, every array element and map entry against the shared element
    // schema — so struct $defs reachable only through containers keep their
    // leaf defaults.
    const auto result = json::schema_string<defaults_containers>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":7})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("beta":{"type":"string","default":"cell"})") != std::string::npos);
    EXPECT_TRUE(result.find(R"("gamma":{"type":"boolean","default":true})") != std::string::npos);
    // The required container properties themselves stay bare.
    EXPECT_TRUE(
        result.find(R"("pool":{"type":"array","items":{"$ref":"#/$defs/defaults_elem_a"}})") !=
        std::string::npos);
}

TEST_CASE(defaults_container_root) {
    // A container root merges its schema shape (std::array reflects as a
    // tuple: prefixItems) into the top-level object; the walk pairs it with
    // the encoded array document and still reaches the element $def.
    const auto result = json::schema_string<std::array<defaults_elem_a, 2>>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":7})") != std::string::npos);
}

TEST_CASE(defaults_repr_backed_root) {
    // kind_of on the raw class reports unknown, but its repr resolves to a
    // struct: the guard judges the JSON-resolved representation, so the pass
    // runs and the representation's defaulted member carries its value.
    const auto result = json::schema_string<json_schema_reprd_root>().value();
    EXPECT_TRUE(result.find(R"("maximum":2147483647,"default":5})") != std::string::npos);
}

struct defaults_enum_config {
    [[maybe_unused]] constexpr static auto enum_repr = codec::enum_repr::String;
    using field_rename = naming::rename_policy::lower_camel;
};

TEST_CASE(defaults_enum_and_rename_with_config) {
    // The default rides through the real encoder: the enum's String repr and
    // the field rename both shape the annotated value and its property name.
    const auto result = json::schema_string<defaults_with_enum, defaults_enum_config>().value();
    EXPECT_TRUE(result.find(R"("logLevel":{"enum":["Low","High"],"default":"High"})") !=
                std::string::npos);
}

TEST_CASE(defaults_type_erased_absent) {
    // The type-erased entry has no T to default-construct, so no defaults.
    const auto result = json::schema_string(type_info_of<defaults_root>()).value();
    EXPECT_TRUE(result.find(R"("default")") == std::string::npos);
}

};  // TEST_SUITE(serde_json_schema)

}  // namespace

}  // namespace kota::meta
