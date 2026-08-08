#include <optional>
#include <string>

#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"
#include "kota/codec/json/schema.h"

namespace kota::codec {

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

struct negative_pred {
    constexpr bool operator()(const int& value, bool is_serialize) const {
        return is_serialize && value < 0;
    }
};

struct annotated_payload {
    int id = 0;

    KOTATSU_ANNOTATE(rename = "displayName", alias = {"name"})<std::string> display_name;

    KOTATSU_ANNOTATE(skip = true)<int> internal_id = 0;

    KOTATSU_ANNOTATE(skip_if = skip_when::none)<std::optional<std::string>> note;

    KOTATSU_ANNOTATE(flatten = true)<profile_info> profile;

    KOTATSU_ANNOTATE(enum_string = type<meta::rename_policy::lower_camel>)<access_level> level =
        access_level::admin;
};

struct defaulted_payload {
    int id = 0;

    KOTATSU_ANNOTATE(defaulted = true)<int> retries = 3;
};

struct custom_skip_payload {
    KOTATSU_ANNOTATE(skip_if = type<negative_pred>)<int> score = 0;
};

struct documented_payload {
    KOTATSU_ANNOTATE(description = "Numeric identifier.", idx = 1u)<int> id = 0;

    std::string name;
};

TEST_SUITE(serde_annotate_macro) {

TEST_CASE(serialize_annotated_fields) {
    annotated_payload input{};
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

TEST_CASE(deserialize_annotated_fields) {
    annotated_payload parsed{};
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

TEST_CASE(defaulted_field_may_be_absent) {
    defaulted_payload parsed{};
    auto status = from_json(R"({"id":1})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.id, 1);
    EXPECT_EQ(parsed.retries, 3);

    defaulted_payload strict{};
    auto missing_required = from_json(R"({"retries":9})", strict);
    EXPECT_FALSE(missing_required.has_value());
}

TEST_CASE(custom_skip_predicate_applies) {
    custom_skip_payload input{};
    input.score = -5;
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({})");

    input.score = 5;
    encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"score":5})");
}

TEST_CASE(description_is_wire_transparent_and_in_schema) {
    documented_payload input{.id = 7, .name = "alice"};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"id":7,"name":"alice"})");

    auto schema = json::schema_string<documented_payload>();
    ASSERT_TRUE(schema.has_value());
    EXPECT_TRUE(schema->find(R"("description":"Numeric identifier.")") != std::string::npos);
}

TEST_CASE(annotated_and_bare_use_share_type_info) {
    // The spec attr is a field-local attr: it must not fork the type_info
    // instance of the underlying type.
    using annotated = decltype(annotated_payload{}.profile);
    EXPECT_EQ(&meta::type_info_of<annotated>(), &meta::type_info_of<profile_info>());
}

};  // TEST_SUITE(serde_annotate_macro)

}  // namespace

}  // namespace kota::codec
