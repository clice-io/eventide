#include <string>

#include "eventide/zest/zest.h"
#include "eventide/serde/json/deserializer.h"
#include "eventide/serde/json/json.h"
#include "eventide/serde/json/serializer.h"
#include "eventide/serde/serde/attrs.h"
#include "eventide/serde/serde/config.h"
#include "eventide/serde/serde/serde.h"

namespace eventide::serde {

namespace {

using json::from_json;
using json::to_json;

struct nested_payload {
    int some_value = 0;
};

struct protocol_payload {
    int request_id = 0;
    std::string user_name;
    nested_payload nested_info{};
};

struct rename_override_payload {
    rename<std::string, "uid"> user_name;
    int request_id = 0;
};

struct ambiguous_camel_payload {
    int user_id = 0;
    int userId = 0;
};

struct camel_config {
    using field_rename = rename_policy::lower_camel;
};

struct upper_camel_config {
    using field_rename = rename_policy::upper_camel;
};

struct upper_snake_config {
    using field_rename = rename_policy::upper_snake;
};

struct lower_snake_config {
    using field_rename = rename_policy::lower_snake;
};

enum class permission_level {
    read_only,
    read_write,
    full_access,
};

struct enum_string_config_upper_snake {
    using enum_rename = rename_policy::upper_snake;
};

struct enum_string_config_lower_camel {
    using enum_rename = rename_policy::lower_camel;
};

TEST_SUITE(serde_simdjson_config) {

TEST_CASE(default_identity_rename) {
    protocol_payload input{
        .request_id = 7,
        .user_name = "alice",
        .nested_info = {.some_value = 3},
    };

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"request_id":7,"user_name":"alice","nested_info":{"some_value":3}})");

    protocol_payload parsed{};
    auto status =
        from_json(R"({"request_id":7,"user_name":"alice","nested_info":{"some_value":3}})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.request_id, 7);
    EXPECT_EQ(parsed.user_name, "alice");
    EXPECT_EQ(parsed.nested_info.some_value, 3);
}

TEST_CASE(lower_camel_rename) {
    protocol_payload input{
        .request_id = 8,
        .user_name = "bob",
        .nested_info = {.some_value = 11},
    };

    auto encoded = to_json<camel_config>(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"requestId":8,"userName":"bob","nestedInfo":{"someValue":11}})");

    protocol_payload parsed{};
    auto status =
        from_json<camel_config>(R"({"requestId":8,"userName":"bob","nestedInfo":{"someValue":11}})",
                                parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.request_id, 8);
    EXPECT_EQ(parsed.user_name, "bob");
    EXPECT_EQ(parsed.nested_info.some_value, 11);
}

TEST_CASE(mixed_configs) {
    protocol_payload input{
        .request_id = 9,
        .user_name = "carol",
        .nested_info = {.some_value = 21},
    };

    auto camel_encoded = to_json<camel_config>(input);
    ASSERT_TRUE(camel_encoded.has_value());
    EXPECT_EQ(*camel_encoded,
              R"({"requestId":9,"userName":"carol","nestedInfo":{"someValue":21}})");

    auto default_encoded = to_json(input);
    ASSERT_TRUE(default_encoded.has_value());
    EXPECT_EQ(*default_encoded,
              R"({"request_id":9,"user_name":"carol","nested_info":{"some_value":21}})");
}

TEST_CASE(config_with_attr_override) {
    rename_override_payload renamed{};
    renamed.user_name = "id-1";
    renamed.request_id = 5;
    auto encoded = to_json<camel_config>(renamed);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"uid":"id-1","requestId":5})");

    rename_override_payload parsed{};
    auto status = from_json<camel_config>(R"({"uid":"id-2","requestId":6})", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.user_name, "id-2");
    EXPECT_EQ(parsed.request_id, 6);
}

TEST_CASE(rename_collision_fails) {
    ambiguous_camel_payload parsed{};
    auto status = from_json<camel_config>(R"({"userId":1})", parsed);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), json::error_kind::invalid_state);
}

TEST_CASE(to_string_with_config) {
    protocol_payload input{.request_id = 5, .user_name = "eve", .nested_info = {.some_value = 1}};
    auto encoded = json::to_string<camel_config>(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"requestId":5,"userName":"eve","nestedInfo":{"someValue":1}})");
}

TEST_CASE(parse_with_config) {
    protocol_payload parsed{};
    auto status = json::parse<camel_config>(
        R"({"requestId":3,"userName":"dan","nestedInfo":{"someValue":7}})",
        parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.request_id, 3);
    EXPECT_EQ(parsed.user_name, "dan");
    EXPECT_EQ(parsed.nested_info.some_value, 7);
}

TEST_CASE(parse_value_with_config) {
    auto result = json::parse<protocol_payload, camel_config>(
        R"({"requestId":2,"userName":"fay","nestedInfo":{"someValue":9}})");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request_id, 2);
    EXPECT_EQ(result->user_name, "fay");
    EXPECT_EQ(result->nested_info.some_value, 9);
}

TEST_CASE(upper_camel_rename) {
    protocol_payload input{
        .request_id = 10,
        .user_name = "grace",
        .nested_info = {.some_value = 5},
    };

    auto encoded = to_json<upper_camel_config>(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"RequestId":10,"UserName":"grace","NestedInfo":{"SomeValue":5}})");

    protocol_payload parsed{};
    auto status = from_json<upper_camel_config>(
        R"({"RequestId":10,"UserName":"grace","NestedInfo":{"SomeValue":5}})",
        parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.request_id, 10);
    EXPECT_EQ(parsed.user_name, "grace");
    EXPECT_EQ(parsed.nested_info.some_value, 5);
}

TEST_CASE(upper_snake_rename) {
    protocol_payload input{
        .request_id = 11,
        .user_name = "henry",
        .nested_info = {.some_value = 6},
    };

    auto encoded = to_json<upper_snake_config>(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"REQUEST_ID":11,"USER_NAME":"henry","NESTED_INFO":{"SOME_VALUE":6}})");

    protocol_payload parsed{};
    auto status = from_json<upper_snake_config>(
        R"({"REQUEST_ID":11,"USER_NAME":"henry","NESTED_INFO":{"SOME_VALUE":6}})",
        parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.request_id, 11);
    EXPECT_EQ(parsed.user_name, "henry");
    EXPECT_EQ(parsed.nested_info.some_value, 6);
}

TEST_CASE(lower_snake_rename) {
    // lower_snake is identity for already-snake_case field names.
    protocol_payload input{
        .request_id = 12,
        .user_name = "iris",
        .nested_info = {.some_value = 7},
    };

    auto encoded = to_json<lower_snake_config>(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"request_id":12,"user_name":"iris","nested_info":{"some_value":7}})");

    protocol_payload parsed{};
    auto status = from_json<lower_snake_config>(
        R"({"request_id":12,"user_name":"iris","nested_info":{"some_value":7}})",
        parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed.request_id, 12);
    EXPECT_EQ(parsed.user_name, "iris");
    EXPECT_EQ(parsed.nested_info.some_value, 7);
}

TEST_CASE(enum_string_upper_snake_policy) {
    // enum_string<E, Policy> — default Policy is lower_camel.
    // With upper_snake policy, multi-word enum names are uppercased with underscores.
    enum_string<permission_level, rename_policy::upper_snake> level = permission_level::read_only;

    auto encoded = to_json(level);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("READ_ONLY")");

    enum_string<permission_level, rename_policy::upper_snake> parsed =
        permission_level::full_access;
    auto status = from_json(R"("READ_WRITE")", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed, permission_level::read_write);
}

TEST_CASE(enum_string_lower_snake_policy) {
    // With lower_snake policy, multi-word enum names stay lower_snake.
    // The default (lower_camel) would produce "readOnly" vs lower_snake "read_only".
    enum_string<permission_level, rename_policy::lower_snake> level = permission_level::full_access;

    auto encoded = to_json(level);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("full_access")");

    enum_string<permission_level, rename_policy::lower_snake> parsed =
        permission_level::full_access;
    auto status = from_json(R"("read_write")", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed, permission_level::read_write);
}

TEST_CASE(enum_string_lower_camel_policy) {
    // Explicit lower_camel policy — multi-word enum names become camelCase.
    enum_string<permission_level, rename_policy::lower_camel> level = permission_level::read_only;

    auto encoded = to_json(level);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("readOnly")");

    enum_string<permission_level, rename_policy::lower_camel> parsed =
        permission_level::full_access;
    auto status = from_json(R"("readWrite")", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed, permission_level::read_write);
}

TEST_CASE(enum_string_upper_camel_policy) {
    // upper_camel policy for enum values: first letter of each word capitalized.
    enum_string<permission_level, rename_policy::upper_camel> level = permission_level::full_access;

    auto encoded = to_json(level);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("FullAccess")");

    enum_string<permission_level, rename_policy::upper_camel> parsed =
        permission_level::full_access;
    auto status = from_json(R"("ReadOnly")", parsed);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(parsed, permission_level::read_only);
}

};  // TEST_SUITE(serde_simdjson_config)

}  // namespace

}  // namespace eventide::serde
