#include <optional>
#include <string>

#include "eventide/zest/zest.h"
#include "eventide/serde/serde.h"
#include "eventide/serde/simdjson/deserializer.h"
#include "eventide/serde/simdjson/serializer.h"

namespace eventide::serde {

namespace ext_attr {

template <fixed_string Name>
struct json_name {};

}  // namespace ext_attr

template <fixed_string Name>
struct attr_hook<ext_attr::json_name<Name>> {
    template <typename S, typename V, typename Next>
    constexpr static decltype(auto) process(serialize_field_ctx<S, V> ctx, Next&& next) {
        ctx.name = Name;
        return std::forward<Next>(next)(ctx);
    }

    template <typename D, typename V, typename Next>
    constexpr static decltype(auto) process(deserialize_field_probe_ctx<D, V> ctx, Next&& next) {
        ctx.field_name = Name;
        return std::forward<Next>(next)(ctx);
    }
};

namespace {

using json::simd::from_json;
using json::simd::to_json;

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

struct custom_attr_payload {
    annotate<std::string, ext_attr::json_name<"handle">> nickname;
};

TEST_SUITE(serde_simdjson_attrs) {

TEST_CASE(serialize_builtin_attrs) {
    builtin_attr_payload input{};
    input.id = 7;
    static_cast<std::string&>(input.display_name) = "alice";
    static_cast<int&>(input.internal_id) = 999;
    static_cast<std::optional<std::string>&>(input.note) = std::nullopt;
    input.profile.first = "Alice";
    input.profile.age = 30;
    static_cast<access_level&>(input.level) = access_level::admin;

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded,
              R"({"id":7,"displayName":"alice","first":"Alice","age":30,"level":"admin"})");
}

TEST_CASE(deserialize_builtin_attrs) {
    builtin_attr_payload parsed{};
    static_cast<int&>(parsed.internal_id) = 321;

    auto status = from_json(
        R"({"id":9,"name":"bob","first":"Bob","age":21,"level":"viewer","internal_id":100,"note":"x"})",
        parsed);
    ASSERT_TRUE(status.has_value());

    EXPECT_EQ(parsed.id, 9);
    EXPECT_EQ(static_cast<const std::string&>(parsed.display_name), "bob");
    EXPECT_EQ(parsed.profile.first, "Bob");
    EXPECT_EQ(parsed.profile.age, 21);
    EXPECT_EQ(static_cast<access_level>(parsed.level), access_level::viewer);
    EXPECT_EQ(static_cast<int>(parsed.internal_id), 321);
    ASSERT_TRUE(static_cast<const std::optional<std::string>&>(parsed.note).has_value());
    EXPECT_EQ(*static_cast<const std::optional<std::string>&>(parsed.note), "x");
}

TEST_CASE(custom_attr_hook_specialization) {
    custom_attr_payload input{};
    static_cast<std::string&>(input.nickname) = "neo";

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"handle":"neo"})");

    custom_attr_payload parsed{};
    auto status = from_json(R"({"handle":"trinity"})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(static_cast<const std::string&>(parsed.nickname), "trinity");
}

TEST_CASE(top_level_annotated_value_enum_string) {
    enum_string<access_level> level = access_level::admin;
    auto encoded = to_json(level);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("admin")");

    enum_string<access_level> parsed = access_level::admin;
    auto status = from_json(R"("viewer")", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(static_cast<access_level>(parsed), access_level::viewer);
}

};  // TEST_SUITE(serde_simdjson_attrs)

}  // namespace

}  // namespace eventide::serde
