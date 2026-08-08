#include <cstdio>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"

// Downstream regression fixtures: outside namespace kota, `rename` must not be
// ambiguous with ::rename from <cstdio>, and the macro must work inside class
// templates.
namespace kotatsu_annotate_downstream {

struct config {
    KOTATSU_ANNOTATE(rename = "compileCommands")<int> compile_commands = 0;
};

template <typename T>
struct box {
    KOTATSU_ANNOTATE(alias = {"v"})<T> value;
};

}  // namespace kotatsu_annotate_downstream

namespace kota::codec {

namespace {

using json::from_json;
using json::to_json;

struct negative_pred {
    constexpr bool operator()(const int& value, bool is_serialize) const {
        return is_serialize && value < 0;
    }
};

/// Adapter: encode int on the wire as its decimal string representation.
struct int_string_adapter {
    using wire_type = std::string;

    static auto to_wire(int value) -> std::string {
        return std::to_string(value);
    }

    static auto from_wire(std::string wire) -> int {
        return wire.empty() ? 0 : std::stoi(wire);
    }
};

/// Strong id encoded as its underlying string via `as`.
struct user_id {
    std::string raw;

    user_id() = default;

    user_id(std::string s) : raw(std::move(s)) {}

    operator std::string() const {
        return raw;
    }
};

struct profile_info {
    std::string first;
    int age = 0;
};

struct defaulted_payload {
    int id = 0;

    KOTATSU_ANNOTATE(defaulted = true)<int> retries = 3;
};

struct custom_skip_payload {
    KOTATSU_ANNOTATE(skip_if = type<negative_pred>)<int> score = 0;
};

struct builtin_skip_payload {
    KOTATSU_ANNOTATE(skip_if = skip_when::empty)<std::vector<int>> tags;

    KOTATSU_ANNOTATE(skip_if = skip_when::default_value)<int> generation = 0;
};

struct adapted_payload {
    KOTATSU_ANNOTATE(with = type<int_string_adapter>)<int> encoded = 0;

    KOTATSU_ANNOTATE(as = type<std::string>)<user_id> owner;
};

struct flattened_payload {
    KOTATSU_ANNOTATE(flatten = true)<profile_info> profile;
};

struct circle {
    double radius = 0;
};

struct rect {
    double width = 0;
};

KOTATSU_ANNOTATION(shape_annotation, tag = "kind", tag_names = {"circle", "rect"});
using shape = meta::annotate<shape_annotation>::type<std::variant<circle, rect>>;

KOTATSU_ANNOTATION(camel_annotation, rename_all = casing::lower_camel, deny_unknown_fields = true);

struct wide_payload {
    std::string user_name;
    int user_age = 0;
};

using camel_payload = meta::annotate<camel_annotation>::type<wide_payload>;

struct shape_holder {
    KOTATSU_ANNOTATE(tag = "kind",
                     tag_names = {"circle", "rect"})<std::variant<circle, rect>> shape;
};

TEST_SUITE(serde_annotate_macro) {

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

TEST_CASE(builtin_skip_conditions_apply) {
    builtin_skip_payload input{};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({})");

    input.tags = std::vector{1, 2};
    input.generation = 5;
    encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"tags":[1,2],"generation":5})");

    builtin_skip_payload parsed{};
    parsed.generation = 7;
    auto absent_ok = from_json(R"({})", parsed);
    ASSERT_TRUE(absent_ok.has_value());
    EXPECT_TRUE(parsed.tags.empty());
    EXPECT_EQ(parsed.generation, 7);

    auto status = from_json(R"({"tags":[3],"generation":9})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.tags, std::vector{3});
    EXPECT_EQ(parsed.generation, 9);
}

TEST_CASE(with_adapter_and_as_target_roundtrip) {
    adapted_payload input{};
    input.encoded = 42;
    input.owner = user_id{"alice"};

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"encoded":"42","owner":"alice"})");

    adapted_payload parsed{};
    auto status = from_json(R"({"encoded":"17","owner":"bob"})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.encoded, 17);
    EXPECT_EQ(parsed.owner.raw, "bob");
}

TEST_CASE(macro_works_outside_kota_namespace) {
    kotatsu_annotate_downstream::config input{};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"compileCommands":0})");

    kotatsu_annotate_downstream::box<int> boxed{};
    auto status = from_json(R"({"v":7})", boxed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(boxed.value, 7);
}

TEST_CASE(named_annotation_tags_variant) {
    shape input{rect{2.5}};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"kind":"rect","width":2.5})");

    shape parsed;
    auto status = from_json(R"({"kind":"circle","radius":1.5})", parsed);
    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(parsed.index(), 0u);
    EXPECT_EQ(std::get<circle>(parsed).radius, 1.5);
}

TEST_CASE(named_annotation_renames_and_denies_unknown) {
    camel_payload input;
    input.user_name = "alice";
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"userName":"alice","userAge":0})");

    camel_payload parsed;
    auto status = from_json(R"({"userName":"bob","userAge":3})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.user_name, "bob");
    EXPECT_EQ(parsed.user_age, 3);

    auto unknown = from_json(R"({"userName":"bob","extra":1})", parsed);
    EXPECT_FALSE(unknown.has_value());
}

TEST_CASE(field_annotation_accepts_struct_entries) {
    shape_holder holder;
    holder.shape = circle{4.0};
    auto encoded = to_json(holder);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"shape":{"kind":"circle","radius":4.0}})");

    shape_holder parsed;
    auto status = from_json(R"({"shape":{"kind":"rect","width":6.0}})", parsed);
    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(parsed.shape.index(), 1u);
    EXPECT_EQ(std::get<rect>(parsed.shape).width, 6.0);
}

TEST_CASE(annotated_and_bare_use_share_type_info) {
    // The spec attr is a field-local attr: it must not fork the type_info
    // instance of the underlying type.
    using annotated = decltype(flattened_payload{}.profile);
    EXPECT_EQ(&meta::type_info_of<annotated>(), &meta::type_info_of<profile_info>());
}

};  // TEST_SUITE(serde_annotate_macro)

}  // namespace

}  // namespace kota::codec
