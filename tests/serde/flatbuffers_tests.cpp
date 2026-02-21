#if __has_include(<flatbuffers/flexbuffers.h>)

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/serde/flatbuffers/flex/deserializer.h"
#include "eventide/serde/flatbuffers/flex/serializer.h"

namespace eventide::serde {

namespace {

using flex::from_flatbuffer;
using flex::to_flatbuffer;

struct person {
    int id;
    std::string name;
    std::vector<int> scores;
};

struct person_with_extra {
    int id;
    std::string name;
    std::vector<int> scores;
    int extra;
};

struct annotated_person {
    int id;
    rename_alias<std::string, "displayName", "name"> name;
    skip<int> internal_id;
    skip_if_none<std::string> note;
};

struct public_person {
    int id;
    std::string displayName;
};

TEST_SUITE(serde_flatbuffers) {

TEST_CASE(roundtrip_vector) {
    const std::vector<int> input{1, 2, 3, 5, 8};
    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    std::vector<int> output;
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(roundtrip_map) {
    const std::map<std::string, std::vector<int>> input{
        {"a", {1, 2}},
        {"b", {3}   }
    };
    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    std::map<std::string, std::vector<int>> output;
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(deserialize_struct_with_unknown_fields) {
    const person_with_extra input{
        .id = 7,
        .name = "alice",
        .scores = {10, 20, 30},
        .extra = 99,
    };
    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    person output{};
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output.id, 7);
    EXPECT_EQ(output.name, "alice");
    EXPECT_EQ(output.scores, std::vector<int>({10, 20, 30}));
}

TEST_CASE(serialize_annotated_fields) {
    annotated_person input{};
    input.id = 42;
    static_cast<std::string&>(input.name) = "bob";
    static_cast<int&>(input.internal_id) = 1024;
    static_cast<std::optional<std::string>&>(input.note) = std::nullopt;

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    public_person output{};
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output.id, 42);
    EXPECT_EQ(output.displayName, "bob");
}

TEST_CASE(roundtrip_bytes_blob) {
    const std::array<std::byte, 4> input{
        std::byte{0},
        std::byte{1},
        std::byte{127},
        std::byte{255},
    };
    auto encoded = to_flatbuffer(std::span<const std::byte>(input));
    ASSERT_TRUE(encoded.has_value());

    std::vector<std::byte> output;
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(output.size(), 4U);
    EXPECT_EQ(std::to_integer<int>(output[0]), 0);
    EXPECT_EQ(std::to_integer<int>(output[1]), 1);
    EXPECT_EQ(std::to_integer<int>(output[2]), 127);
    EXPECT_EQ(std::to_integer<int>(output[3]), 255);
}

TEST_CASE(optional_and_return_value_overload) {
    const std::optional<int> some = 9;
    auto encoded_some = to_flatbuffer(some);
    ASSERT_TRUE(encoded_some.has_value());

    auto decoded_some = from_flatbuffer<std::optional<int>>(*encoded_some);
    ASSERT_TRUE(decoded_some.has_value());
    EXPECT_EQ(*decoded_some, std::optional<int>(9));

    const std::optional<int> none = std::nullopt;
    auto encoded_none = to_flatbuffer(none);
    ASSERT_TRUE(encoded_none.has_value());

    auto decoded_none = from_flatbuffer<std::optional<int>>(*encoded_none);
    ASSERT_TRUE(decoded_none.has_value());
    EXPECT_EQ(*decoded_none, std::optional<int>(std::nullopt));
}

};  // TEST_SUITE(serde_flatbuffers)

}  // namespace

}  // namespace eventide::serde

#endif
