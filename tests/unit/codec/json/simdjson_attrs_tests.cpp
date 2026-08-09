#include <optional>
#include <string>

#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"

namespace kota::codec {

using namespace meta;

namespace {

using json::from_json;
using json::to_json;

enum class access_level {
    admin,
    viewer,
};

struct access_level_enum_string_tag {
    constexpr static auto spec =
        make_spec(dsl::enum_string = dsl::type<rename_policy::lower_camel>);
};

using access_level_enum_string = annotate<access_level_enum_string_tag>::type<access_level>;

struct profile_info {
    std::string first;
    int age = 0;
};

struct builtin_attr_payload {
    int id = 0;
    KOTATSU_ANNOTATE(rename = "displayName", alias = {"name"})<std::string> display_name;
    KOTATSU_ANNOTATE(skip = true)<int> internal_id;
    KOTATSU_ANNOTATE(skip_if = skip_when::none)<std::optional<std::string>> note;
    KOTATSU_ANNOTATE(flatten = true)<profile_info> profile;
    KOTATSU_ANNOTATE(enum_string = type<rename_policy::lower_camel>)<access_level> level;
};

struct custom_rename_payload {
    KOTATSU_ANNOTATE(rename = "handle")<std::string> nickname;
};

struct alias_conflict_payload {
    KOTATSU_ANNOTATE(alias = {"dup"})<int> left = 0;
    KOTATSU_ANNOTATE(alias = {"dup"})<int> right = 0;
};

struct skip_unsupported_payload {
    int id = 0;
    KOTATSU_ANNOTATE(skip = true)<int*> raw = nullptr;
};

struct documented_payload {
    KOTATSU_ANNOTATE(description = "Numeric identifier.")<int> id = 0;
    std::string name;
};

struct struct_level_payload {
    int user_id = 0;
    int login_count = 0;
};

KOTATSU_ANNOTATION(renamed_struct_level_annotation, rename_all = casing::lower_camel);
using renamed_struct_level_payload =
    annotate<renamed_struct_level_annotation>::type<struct_level_payload>;

KOTATSU_ANNOTATION(strict_renamed_struct_level_annotation,
                   rename_all = casing::lower_camel,
                   deny_unknown_fields = true);
using strict_renamed_struct_level_payload =
    annotate<strict_renamed_struct_level_annotation>::type<struct_level_payload>;

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
    access_level_enum_string level = access_level::admin;
    auto encoded = to_json(level);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("admin")");

    access_level_enum_string parsed = access_level::admin;
    auto status = from_json(R"("viewer")", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed, access_level::viewer);
}

TEST_CASE(top_level_annotated_value_enum_string_unknown_fails) {
    access_level_enum_string parsed = access_level::admin;
    auto status = from_json(R"("unknown")", parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(parsed, access_level::admin);
}

TEST_CASE(alias_conflict_fails_fast) {
    alias_conflict_payload parsed{};
    auto status = from_json(R"({"dup":1})", parsed);
    EXPECT_FALSE(status.has_value());
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
    EXPECT_TRUE(status.error().message.find("unknown field") != std::string::npos);
}

TEST_CASE(description_attr_is_wire_transparent) {
    documented_payload input{.id = 7, .name = "alice"};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"id":7,"name":"alice"})");

    documented_payload parsed{};
    auto status = from_json(R"({"id":3,"name":"bob"})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 3);
    EXPECT_EQ(parsed.name, "bob");
}

};  // TEST_SUITE(serde_simdjson_attrs)

}  // namespace

}  // namespace kota::codec
