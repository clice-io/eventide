#if __has_include(<yaml-cpp/yaml.h>)

#include <optional>
#include <string>
#include <vector>

#include "fixtures/schema/common.h"
#include "kota/zest/zest.h"
#include "kota/codec/yaml/yaml.h"

namespace kota::codec {

namespace {

using yaml::from_yaml;
using yaml::parse;
using yaml::to_string;
using yaml::to_yaml;

using meta::fixtures::Person;
using meta::fixtures::PersonWithScores;
using meta::fixtures::Point2i;

TEST_SUITE(serde_yaml) {

TEST_CASE(struct_roundtrip_with_dom) {
    const Person input{.name = "Alice", .age = 30, .addr = {.city = "Tokyo", .zip = 100}};

    auto dom = to_yaml(input);
    ASSERT_TRUE(dom.has_value());
    ASSERT_TRUE(dom->IsMap());

    Person output{};
    auto status = from_yaml(*dom, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output.name, input.name);
    EXPECT_EQ(output.age, input.age);
    EXPECT_EQ(output.addr.city, input.addr.city);
    EXPECT_EQ(output.addr.zip, input.addr.zip);
}

TEST_CASE(parse_and_to_string_roundtrip) {
    constexpr std::string_view input = R"(
name: Bob
age: 25
addr:
  city: Osaka
  zip: 530
)";

    auto parsed = parse<Person>(input);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->name, "Bob");
    EXPECT_EQ(parsed->age, 25);
    EXPECT_EQ(parsed->addr.city, "Osaka");
    EXPECT_EQ(parsed->addr.zip, 530);

    auto encoded = to_string(*parsed);
    ASSERT_TRUE(encoded.has_value());

    auto reparsed = parse<Person>(*encoded);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->name, parsed->name);
    EXPECT_EQ(reparsed->age, parsed->age);
    EXPECT_EQ(reparsed->addr.city, parsed->addr.city);
    EXPECT_EQ(reparsed->addr.zip, parsed->addr.zip);
}

TEST_CASE(vector_roundtrip) {
    std::vector<Point2i> input = {{1, 2}, {3, 4}, {5, 6}};

    auto dom = to_yaml(input);
    ASSERT_TRUE(dom.has_value());
    ASSERT_TRUE(dom->IsSequence());
    EXPECT_EQ(dom->size(), 3u);

    auto output = from_yaml<std::vector<Point2i>>(*dom);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(output->size(), 3u);
    EXPECT_EQ((*output)[0].x, 1);
    EXPECT_EQ((*output)[0].y, 2);
    EXPECT_EQ((*output)[2].x, 5);
    EXPECT_EQ((*output)[2].y, 6);
}

TEST_CASE(struct_with_vector) {
    const PersonWithScores input{.id = 7, .name = "charlie", .scores = {10, 20, 30}, .active = true};

    auto dom = to_yaml(input);
    ASSERT_TRUE(dom.has_value());

    auto output = from_yaml<PersonWithScores>(*dom);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(output->id, input.id);
    EXPECT_EQ(output->name, input.name);
    EXPECT_EQ(output->scores, input.scores);
    EXPECT_EQ(output->active, input.active);
}

TEST_CASE(parse_error) {
    auto result = parse<Person>("{{{{invalid yaml");
    EXPECT_FALSE(result.has_value());
}

TEST_CASE(type_mismatch) {
    auto node = YAML::Load("[1, 2, 3]");
    Person p{};
    auto result = from_yaml(node, p);
    EXPECT_FALSE(result.has_value());
}

TEST_CASE(optional_present) {
    std::optional<int> input = 42;
    auto dom = to_yaml(input);
    ASSERT_TRUE(dom.has_value());

    auto output = from_yaml<std::optional<int>>(*dom);
    ASSERT_TRUE(output.has_value());
    ASSERT_TRUE(output->has_value());
    EXPECT_EQ(**output, 42);
}

TEST_CASE(optional_absent) {
    std::optional<int> input = std::nullopt;
    auto dom = to_yaml(input);
    ASSERT_TRUE(dom.has_value());

    auto output = from_yaml<std::optional<int>>(*dom);
    ASSERT_TRUE(output.has_value());
    EXPECT_FALSE(output->has_value());
}

TEST_CASE(scalar_string) {
    std::string input = "hello world";
    auto dom = to_yaml(input);
    ASSERT_TRUE(dom.has_value());

    auto output = from_yaml<std::string>(*dom);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, "hello world");
}

TEST_CASE(scalar_bool) {
    auto dom = to_yaml(true);
    ASSERT_TRUE(dom.has_value());

    auto output = from_yaml<bool>(*dom);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, true);
}

TEST_CASE(scalar_double) {
    auto dom = to_yaml(3.14);
    ASSERT_TRUE(dom.has_value());

    auto output = from_yaml<double>(*dom);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(*output > 3.13 && *output < 3.15);
}

TEST_CASE(dynamic_dom_field) {
    auto node = YAML::Load("{name: test, value: 42}");
    YAML::Node captured;
    auto result = from_yaml(node, captured);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(captured["name"].as<std::string>(), "test");
    EXPECT_EQ(captured["value"].as<int>(), 42);
}

};  // TEST_SUITE

}  // namespace

}  // namespace kota::codec

#endif
