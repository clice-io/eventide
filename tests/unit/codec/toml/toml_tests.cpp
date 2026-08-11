#if __has_include(<toml++/toml.hpp>)

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "fixtures/schema/common.h"
#include "kota/zest/zest.h"
#include "kota/codec/toml/toml.h"

namespace kota::codec {

namespace {

using toml::from_toml;
using toml::from_toml_table;
using toml::parse;
using toml::to_string;
using toml::to_toml;

using person = meta::fixtures::PersonWithScores;

struct payload_with_extra {
    int id = 0;
    ::toml::table extra;
};

TEST_SUITE(serde_toml) {

TEST_CASE(struct_roundtrip_with_dom) {
    const person input{
        .id = 7,
        .name = "alice",
        .scores = {1, 2, 3},
        .active = true,
    };

    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());
    ASSERT_TRUE(dom->contains("id"));
    ASSERT_TRUE(dom->contains("name"));
    ASSERT_TRUE(dom->contains("scores"));
    ASSERT_TRUE(dom->contains("active"));

    person output{};
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(parse_and_to_string_roundtrip) {
    constexpr std::string_view input = R"(
id = 9
name = "bob"
scores = [4, 5]
active = true
)";

    auto parsed = parse<person>(input);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->id, 9);
    EXPECT_EQ(parsed->name, "bob");
    EXPECT_EQ(parsed->scores, std::vector<int>({4, 5}));
    EXPECT_EQ(parsed->active, true);

    auto encoded = to_string(*parsed);
    ASSERT_TRUE(encoded.has_value());

    auto reparsed = parse<person>(*encoded);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(*reparsed, *parsed);
}

TEST_CASE(dynamic_dom_field_roundtrip) {
    payload_with_extra input{};
    input.id = 1;
    input.extra.insert_or_assign("city", "shanghai");
    input.extra.insert_or_assign("zip", 200000);

    ::toml::array tags;
    tags.push_back("a");
    tags.push_back("b");
    input.extra.insert_or_assign("tags", std::move(tags));

    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());

    payload_with_extra output{};
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());

    EXPECT_EQ(output.id, 1);
    auto city = output.extra["city"].value<std::string_view>();
    ASSERT_TRUE(city.has_value());
    EXPECT_EQ(*city, "shanghai");

    auto zip = output.extra["zip"].value<std::int64_t>();
    ASSERT_TRUE(zip.has_value());
    EXPECT_EQ(*zip, 200000);

    auto tags_out = output.extra["tags"].as_array();
    ASSERT_TRUE(tags_out != nullptr);
    ASSERT_EQ(tags_out->size(), 2U);
    EXPECT_EQ((*tags_out)[0].value<std::string_view>().value_or(""), "a");
    EXPECT_EQ((*tags_out)[1].value<std::string_view>().value_or(""), "b");
}

TEST_CASE(boxed_root_scalar_and_optional_none) {
    const std::vector<int> values{3, 5, 8};
    auto encoded_values = to_toml(values);
    ASSERT_TRUE(encoded_values.has_value());
    ASSERT_TRUE(encoded_values->contains("__value"));

    std::vector<int> decoded_values{};
    auto decode_values_status = from_toml_table(*encoded_values, decoded_values);
    ASSERT_TRUE(decode_values_status.has_value());
    EXPECT_EQ(decoded_values, values);

    const std::optional<int> none = std::nullopt;
    auto encoded_none = to_toml(none);
    ASSERT_TRUE(encoded_none.has_value());
    EXPECT_TRUE(encoded_none->empty());

    std::optional<int> decoded_none = 42;
    auto decode_none_status = from_toml_table(*encoded_none, decoded_none);
    ASSERT_TRUE(decode_none_status.has_value());
    EXPECT_FALSE(decoded_none.has_value());
}

TEST_CASE(shared_ptr_root_roundtrip) {
    const auto input = std::make_shared<person>(person{
        .id = 3,
        .name = "carol",
        .scores = {9, 9},
        .active = false,
    });

    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());

    std::shared_ptr<person> output;
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(output != nullptr);
    EXPECT_EQ(*output, *input);
}

TEST_CASE(null_shared_ptr_root) {
    const std::shared_ptr<person> input;
    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());
    EXPECT_TRUE(dom->empty());

    auto output = std::make_shared<person>();
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_TRUE(output == nullptr);
}

TEST_CASE(unique_ptr_root_roundtrip) {
    auto input = std::make_unique<person>(person{
        .id = 8,
        .name = "dave",
        .scores = {1},
        .active = true,
    });

    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());

    std::unique_ptr<person> output;
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(output != nullptr);
    EXPECT_EQ(*output, *input);
}

TEST_CASE(optional_root_present_roundtrip) {
    const std::optional<person> input = person{
        .id = 4,
        .name = "erin",
        .scores = {7, 8},
        .active = true,
    };

    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());
    EXPECT_FALSE(dom->contains("__value"));

    std::optional<person> output;
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, *input);
}

TEST_CASE(pointer_to_scalar_root_boxes) {
    // A scalar pointee routes through the boxed root key, same as a bare
    // scalar root.
    const auto input = std::make_shared<int>(7);
    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());
    EXPECT_TRUE(dom->contains("__value"));

    std::shared_ptr<int> output;
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(output != nullptr);
    EXPECT_EQ(*output, 7);
}

TEST_CASE(table_root_symmetry) {
    ::toml::table input;
    input.insert_or_assign("city", "shanghai");
    input.insert_or_assign("zip", 200000);

    // A raw table root becomes the document root itself, not a boxed value.
    auto dom = to_toml(input);
    ASSERT_TRUE(dom.has_value());
    EXPECT_FALSE(dom->contains("__value"));
    EXPECT_EQ((*dom)["city"].value<std::string_view>().value_or(""), "shanghai");
    EXPECT_EQ((*dom)["zip"].value<std::int64_t>().value_or(0), 200000);

    ::toml::table output;
    auto status = from_toml_table(*dom, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_TRUE(output == input);
}

TEST_CASE(tuple_length_errors) {
    // Helper: wrap a toml::array in a boxed root table (__value = arr)
    auto boxed = [](::toml::array arr) {
        ::toml::table tbl;
        tbl.insert_or_assign("__value", std::move(arr));
        return tbl;
    };

    // Too many elements for tuple<int,int>
    {
        auto tbl = boxed(::toml::array{1, 2, 3});
        std::tuple<int, int> t{};
        EXPECT_FALSE(from_toml_table(tbl, t).has_value());
    }

    // Too few elements for tuple<int,int>
    {
        auto tbl = boxed(::toml::array{1});
        std::tuple<int, int> t{};
        EXPECT_FALSE(from_toml_table(tbl, t).has_value());
    }

    // Too many elements for pair<int,int>
    {
        auto tbl = boxed(::toml::array{1, 2, 3});
        std::pair<int, int> p{};
        EXPECT_FALSE(from_toml_table(tbl, p).has_value());
    }

    // Too few elements for pair
    {
        auto tbl = boxed(::toml::array{1});
        std::pair<int, int> p{};
        EXPECT_FALSE(from_toml_table(tbl, p).has_value());
    }

    // Empty array into non-empty tuple
    {
        auto tbl = boxed(::toml::array{});
        std::tuple<int> t{};
        EXPECT_FALSE(from_toml_table(tbl, t).has_value());
    }

    // Non-empty array into empty tuple
    {
        auto tbl = boxed(::toml::array{1});
        std::tuple<> t{};
        EXPECT_FALSE(from_toml_table(tbl, t).has_value());
    }

    // Too many elements for array<int,2>
    {
        auto tbl = boxed(::toml::array{1, 2, 3});
        std::array<int, 2> a{};
        EXPECT_FALSE(from_toml_table(tbl, a).has_value());
    }

    // Too few elements for array<int,2>
    {
        auto tbl = boxed(::toml::array{1});
        std::array<int, 2> a{};
        EXPECT_FALSE(from_toml_table(tbl, a).has_value());
    }

    // Exact match still works
    {
        auto tbl = boxed(::toml::array{1, 2});
        std::tuple<int, int> t{};
        ASSERT_TRUE(from_toml_table(tbl, t).has_value());
        EXPECT_EQ(std::get<0>(t), 1);
        EXPECT_EQ(std::get<1>(t), 2);
    }

    // Type mismatch within tuple
    {
        auto tbl = boxed(::toml::array{1, "x"});
        std::tuple<int, int> t{};
        EXPECT_FALSE(from_toml_table(tbl, t).has_value());
    }
}

};  // TEST_SUITE(serde_toml)

}  // namespace

}  // namespace kota::codec

// ============================================================================
// Format-scoped repr: the TOML backend picks repr<T, toml::format>.
// ============================================================================

namespace kota_toml_format_test {

// Generic form is textual; the toml-scoped override is a bare integer.
struct journal {
    int page = 0;

    auto operator<=>(const journal&) const = default;
};

// The toml-scoped repr resolves to a struct: the representation is
// table-shaped, so it must become the document root itself.
struct diary {
    int page = 0;

    auto operator==(const diary&) const -> bool = default;
};

struct diary_record {
    int page = 0;
};

}  // namespace kota_toml_format_test

namespace kota::meta {

template <>
struct repr<kota_toml_format_test::journal> {
    using type = std::string;

    static type to(const kota_toml_format_test::journal& j) {
        return "p" + std::to_string(j.page);
    }

    static kota_toml_format_test::journal from(const std::string& encoded) {
        return {.page = std::stoi(encoded.substr(1))};
    }
};

template <>
struct repr<kota_toml_format_test::journal, codec::toml::format> {
    using type = std::int64_t;

    static type to(const kota_toml_format_test::journal& j) {
        return j.page;
    }

    static kota_toml_format_test::journal from(type v) {
        return {.page = static_cast<int>(v)};
    }
};

template <>
struct repr<kota_toml_format_test::diary, codec::toml::format> {
    using type = kota_toml_format_test::diary_record;

    static type to(const kota_toml_format_test::diary& d) {
        return {.page = d.page};
    }

    static kota_toml_format_test::diary from(const type& r) {
        return {.page = r.page};
    }
};

}  // namespace kota::meta

namespace kota::codec {

namespace {

using kota_toml_format_test::diary;
using kota_toml_format_test::journal;

struct journal_entry {
    journal j;

    auto operator==(const journal_entry&) const -> bool = default;
};

TEST_SUITE(serde_toml_format_scoped) {

TEST_CASE(format_scoped_repr_selected_by_toml) {
    const journal_entry input{.j = {.page = 41}};

    auto text = toml::to_string(input);
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->find("j = 41") != std::string::npos);

    auto output = toml::parse<journal_entry>(*text);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, input);
}

TEST_CASE(map_keys_follow_toml_scoped_repr) {
    const std::map<journal, int> input{
        {journal{.page = 7},  1},
        {journal{.page = 19}, 2},
    };

    auto text = toml::to_string(input);
    ASSERT_TRUE(text.has_value());
    // Keys travel through the toml-scoped integer repr, not the generic
    // textual one.
    EXPECT_TRUE(text->find("p7") == std::string::npos);

    auto output = toml::parse<std::map<journal, int>>(*text);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, input);
}

TEST_CASE(top_level_scalar_repr_boxed_under_root_key) {
    // journal's toml repr is a scalar, so the root routing must box it under
    // the root key instead of dumping raw struct fields into the table.
    const journal input{.page = 12};

    auto text = toml::to_string(input);
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->find("page") == std::string::npos);

    auto output = toml::parse<journal>(*text);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, input);
}

TEST_CASE(top_level_table_shaped_repr_becomes_root) {
    // diary's toml repr resolves to a struct: the represented fields form the
    // root table directly.
    const diary input{.page = 3};

    auto text = toml::to_string(input);
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->find("page = 3") != std::string::npos);
    EXPECT_TRUE(text->find(std::string(toml::detail::boxed_root_key)) == std::string::npos);

    auto output = toml::parse<diary>(*text);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, input);
}

};  // TEST_SUITE(serde_toml_format_scoped)

}  // namespace

}  // namespace kota::codec

#endif
