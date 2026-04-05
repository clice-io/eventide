#include <optional>
#include <string>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/serde/json/deserializer.h"
#include "eventide/serde/json/serializer.h"
#include "eventide/serde/serde/serde.h"

namespace eventide::serde {

namespace {

using json::from_json;
using json::to_json;

enum class access_level {
    admin,
    viewer,
};

struct profile_info {
    std::string first;
    int age = 0;
};

struct builtin_attr_payload {
    int id = 0;
    rename_alias<std::string, "displayName", "name"> display_name;
    skip<int> internal_id;
    skip_if_none<std::string> note;
    flatten<profile_info> profile;
    enum_string<access_level> level;
};

struct custom_rename_payload {
    rename<std::string, "handle"> nickname;
};

struct alias_conflict_payload {
    alias<int, "dup"> left = 0;
    alias<int, "dup"> right = 0;
};

struct skip_unsupported_payload {
    int id = 0;
    skip<int*> raw = nullptr;
};

// skip_if_empty: field is omitted from JSON when the container is empty on serialize.
// Note: skip_if_empty does NOT include schema::default_value, so the field is required
// during deserialization. Absent field → missing_field error.
struct skip_if_empty_payload {
    int id = 0;
    skip_if_empty<std::vector<int>> items;
};

// skip_if_default: field is omitted from JSON when equal to T{} on serialize.
// Includes schema::default_value so absent field during deser → keeps default value (T{}).
struct skip_if_default_payload {
    int id = 0;
    skip_if_default<int> count;
};

// literal<"value">: purely metadata. It does NOT rename the field on the wire
// (neither during serialization nor deserialization). The field is serialized
// and matched by its struct field name. The literal name is available as
// compile-time metadata (e.g. for schema generation) but is not enforced.
struct literal_payload {
    int id = 0;
    literal<std::string, "type"> kind;
};

// Multiple aliases on the same field using rename_alias<T, Name, AliasNames...>.
struct multi_alias_payload {
    int id = 0;
    rename_alias<int, "primary", "alt1", "alt2"> value = 0;
};

// Two flattened sub-structs.
struct flat_a {
    int x = 0;
    int y = 0;
};

struct flat_b {
    std::string label;
    int z = 0;
};

struct double_flatten_payload {
    flatten<flat_a> a;
    flatten<flat_b> b;
};

// deny_unknown_fields parent with a nested (non-strict) struct field.
struct nested_inner {
    int value = 0;
};

struct deny_unknown_with_nested_payload {
    int id = 0;
    nested_inner inner;
};

using strict_deny_unknown_with_nested_payload =
    annotation<deny_unknown_with_nested_payload, schema::deny_unknown_fields>;

struct struct_level_payload {
    int user_id = 0;
    int login_count = 0;
};

using renamed_struct_level_payload =
    annotation<struct_level_payload, schema::rename_all<rename_policy::lower_camel>>;
using strict_renamed_struct_level_payload =
    annotation<struct_level_payload,
               schema::rename_all<rename_policy::lower_camel>,
               schema::deny_unknown_fields>;

TEST_SUITE(serde_simdjson_attrs) {

TEST_CASE(serialize_builtin_attrs) {
    builtin_attr_payload input{};
    input.id = 7;
    input.display_name = "alice";
    input.internal_id = 999;
    input.note = std::nullopt;
    input.profile.first = "Alice";
    input.profile.age = 30;
    input.level = access_level::admin;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded,
              R"({"id":7,"displayName":"alice","first":"Alice","age":30,"level":"admin"})");
}

TEST_CASE(deserialize_builtin_attrs) {
    builtin_attr_payload parsed{};
    parsed.internal_id = 321;

    auto status = from_json(
        R"({"id":9,"name":"bob","first":"Bob","age":21,"level":"viewer","internal_id":100,"note":"x"})",
        parsed);
    ASSERT_TRUE(status.has_value());

    EXPECT_EQ(parsed.id, 9);
    EXPECT_EQ(parsed.display_name, "bob");
    EXPECT_EQ(parsed.profile.first, "Bob");
    EXPECT_EQ(parsed.profile.age, 21);
    EXPECT_EQ(parsed.level, access_level::viewer);
    EXPECT_EQ(parsed.internal_id, 321);
    EXPECT_EQ(parsed.note, std::optional<std::string>{"x"});
}

TEST_CASE(deserialize_builtin_attrs_unknown_enum_fails) {
    builtin_attr_payload parsed{};
    parsed.level = access_level::admin;

    auto status =
        from_json(R"({"id":9,"displayName":"bob","first":"Bob","age":21,"level":"super_admin"})",
                  parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(parsed.level, access_level::admin);
}

TEST_CASE(rename_attr_serialization) {
    custom_rename_payload input{};
    input.nickname = "neo";

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"handle":"neo"})");

    custom_rename_payload parsed{};
    auto status = from_json(R"({"handle":"trinity"})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.nickname, "trinity");
}

TEST_CASE(top_level_annotated_value_enum_string) {
    enum_string<access_level> level = access_level::admin;
    auto encoded = to_json(level);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("admin")");

    enum_string<access_level> parsed = access_level::admin;
    auto status = from_json(R"("viewer")", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed, access_level::viewer);
}

TEST_CASE(top_level_annotated_value_enum_string_unknown_fails) {
    enum_string<access_level> parsed = access_level::admin;
    auto status = from_json(R"("unknown")", parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(parsed, access_level::admin);
}

TEST_CASE(alias_conflict_fails_fast) {
    alias_conflict_payload parsed{};
    auto status = from_json(R"({"dup":1})", parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), json::error_kind::invalid_state);
}

TEST_CASE(skip_field_does_not_require_deserializer) {
    skip_unsupported_payload parsed{};
    auto status = from_json(R"({"id":17})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 17);
    EXPECT_EQ(parsed.raw, nullptr);
}

TEST_CASE(annotated_struct_rename_all_applies) {
    renamed_struct_level_payload input{};
    input.user_id = 7;
    input.login_count = 12;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"userId":7,"loginCount":12})");

    renamed_struct_level_payload parsed{};
    auto status = from_json(R"({"userId":3,"loginCount":4})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.user_id, 3);
    EXPECT_EQ(parsed.login_count, 4);
}

TEST_CASE(annotated_struct_deny_unknown_fields_applies) {
    strict_renamed_struct_level_payload parsed{};

    auto status = from_json(R"({"userId":3,"loginCount":4,"extra":9})", parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), json::error_kind::type_mismatch);
}

// --- skip_if_empty ---

TEST_CASE(skip_if_empty_omits_empty_field_on_serialize) {
    skip_if_empty_payload input{};
    input.id = 1;
    // items is default-constructed (empty vector)

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"id":1})");
}

TEST_CASE(skip_if_empty_includes_non_empty_field_on_serialize) {
    skip_if_empty_payload input{};
    input.id = 2;
    input.items = {10, 20, 30};

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"id":2,"items":[10,20,30]})");
}

TEST_CASE(skip_if_empty_field_absent_on_deser_is_missing_field_error) {
    // skip_if_empty does not include schema::default_value, so the field is
    // required during deserialization. An absent field yields a missing_field error,
    // which maps to error_kind::type_mismatch (serde_error::missing_field uses
    // Kind::type_mismatch).
    skip_if_empty_payload parsed{};
    auto status = from_json(R"({"id":3})", parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), json::error_kind::type_mismatch);
}

TEST_CASE(skip_if_empty_roundtrip_non_empty) {
    skip_if_empty_payload input{};
    input.id = 4;
    input.items = {1, 2, 3};

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());

    skip_if_empty_payload parsed{};
    auto status = from_json(*encoded, parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 4);
    EXPECT_EQ(parsed.items, (std::vector<int>{1, 2, 3}));
}

// --- skip_if_default ---

TEST_CASE(skip_if_default_omits_default_value_on_serialize) {
    skip_if_default_payload input{};
    input.id = 5;
    // count defaults to 0 (int{})

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"id":5})");
}

TEST_CASE(skip_if_default_includes_non_default_value_on_serialize) {
    skip_if_default_payload input{};
    input.id = 6;
    input.count = 42;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"id":6,"count":42})");
}

TEST_CASE(skip_if_default_field_absent_on_deser_keeps_default) {
    // skip_if_default includes schema::default_value so absent field is allowed.
    // When a field is absent, its current value is left untouched (not reset to T{}).
    skip_if_default_payload parsed{};
    parsed.count = 99;

    auto status = from_json(R"({"id":7})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 7);
    // Field was absent: pre-existing value is preserved (not reset to T{}).
    EXPECT_EQ(parsed.count, 99);
}

TEST_CASE(skip_if_default_roundtrip_non_default) {
    skip_if_default_payload input{};
    input.id = 8;
    input.count = 42;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());

    skip_if_default_payload parsed{};
    auto status = from_json(*encoded, parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 8);
    EXPECT_EQ(parsed.count, 42);
}

// --- literal ---

// schema::literal<Name> is pure compile-time metadata. It does NOT affect the
// wire name used during serialization or deserialization — both use the struct
// field name ("kind") as the key. Value validation is also not performed.
TEST_CASE(literal_roundtrip) {
    literal_payload input{};
    input.id = 9;
    input.kind = "widget";

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    // Wire name is the struct field name "kind", not the literal name "type".
    EXPECT_EQ(*encoded, R"({"id":9,"kind":"widget"})");

    literal_payload parsed{};
    auto status = from_json(*encoded, parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 9);
    EXPECT_EQ(parsed.kind, "widget");
}

TEST_CASE(literal_accepts_any_string_value) {
    // literal validation is not implemented: any string value is accepted.
    // The field is matched by struct field name "kind", not the literal name "type".
    literal_payload parsed{};
    auto status = from_json(R"({"id":10,"kind":"something_else"})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 10);
    EXPECT_EQ(parsed.kind, "something_else");
}

// --- Multiple aliases ---

TEST_CASE(multi_alias_deser_by_primary_name) {
    multi_alias_payload parsed{};
    auto status = from_json(R"({"id":1,"primary":100})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 1);
    EXPECT_EQ(parsed.value, 100);
}

TEST_CASE(multi_alias_deser_by_first_alias) {
    multi_alias_payload parsed{};
    auto status = from_json(R"({"id":2,"alt1":200})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 2);
    EXPECT_EQ(parsed.value, 200);
}

TEST_CASE(multi_alias_deser_by_second_alias) {
    multi_alias_payload parsed{};
    auto status = from_json(R"({"id":3,"alt2":300})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 3);
    EXPECT_EQ(parsed.value, 300);
}

TEST_CASE(multi_alias_serializes_with_primary_name) {
    multi_alias_payload input{};
    input.id = 4;
    input.value = 400;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"id":4,"primary":400})");
}

// --- Multiple flatten fields ---

TEST_CASE(double_flatten_serialize_all_fields_at_parent_level) {
    double_flatten_payload input{};
    input.a.x = 1;
    input.a.y = 2;
    input.b.label = "hello";
    input.b.z = 3;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"x":1,"y":2,"label":"hello","z":3})");
}

TEST_CASE(double_flatten_deserialize_populates_both_sub_structs) {
    double_flatten_payload parsed{};
    auto status = from_json(R"({"x":10,"y":20,"label":"world","z":30})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.a.x, 10);
    EXPECT_EQ(parsed.a.y, 20);
    EXPECT_EQ(parsed.b.label, "world");
    EXPECT_EQ(parsed.b.z, 30);
}

TEST_CASE(double_flatten_roundtrip) {
    double_flatten_payload input{};
    input.a.x = 5;
    input.a.y = 6;
    input.b.label = "test";
    input.b.z = 7;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());

    double_flatten_payload parsed{};
    auto status = from_json(*encoded, parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.a.x, 5);
    EXPECT_EQ(parsed.a.y, 6);
    EXPECT_EQ(parsed.b.label, "test");
    EXPECT_EQ(parsed.b.z, 7);
}

// --- deny_unknown_fields with nested struct ---

TEST_CASE(deny_unknown_with_nested_rejects_extra_field_at_parent_level) {
    strict_deny_unknown_with_nested_payload parsed{};
    auto status = from_json(R"({"id":1,"inner":{"value":42},"extra":99})", parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), json::error_kind::type_mismatch);
}

TEST_CASE(deny_unknown_with_nested_accepts_valid_json) {
    strict_deny_unknown_with_nested_payload parsed{};
    auto status = from_json(R"({"id":1,"inner":{"value":42}})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 1);
    EXPECT_EQ(parsed.inner.value, 42);
}

TEST_CASE(deny_unknown_with_nested_allows_extra_field_inside_nested_struct) {
    // deny_unknown_fields only applies to the annotated parent struct.
    // The nested struct (nested_inner) uses default deserialization, which
    // ignores unknown fields.
    strict_deny_unknown_with_nested_payload parsed{};
    auto status = from_json(R"({"id":2,"inner":{"value":7,"extra_inner":99}})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 2);
    EXPECT_EQ(parsed.inner.value, 7);
}

};  // TEST_SUITE(serde_simdjson_attrs)

}  // namespace

}  // namespace eventide::serde
