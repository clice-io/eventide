#if __has_include(<flatbuffers/flatbuffers.h>)

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "fixtures/schema/common.h"
#include "kota/zest/zest.h"
#include "kota/meta/attrs.h"
#include "kota/codec/fbs/fbs.h"
#include "flatbuffers/flatbuffers.h"

namespace kota::codec {

using namespace meta;

namespace {

using fbs::array_view;
using fbs::map_view;
using fbs::table_view;
using fbs::to_bytes;
using fbs::tuple_view;
using fbs::variant_view;

enum class color : std::int32_t { red = 0, green = 1, blue = 2 };

using point = meta::fixtures::Point2i;
using address = meta::fixtures::Address;

struct person {
    std::int32_t id;
    std::string name;
    point pos;
    std::vector<std::int32_t> scores;
    address addr;
};

struct with_skip {
    std::int32_t a;
    KOTATSU_ANNOTATE(skip = true)
    <std::int32_t> internal;
    std::int32_t c;
};

TEST_SUITE(serde_flatbuffers_object) {

TEST_CASE(trivial_struct_field_serializes_as_inline_struct) {
    const person input{
        .id = 7,
        .name = "alice",
        .pos = {.x = 10, .y = 20},
        .scores = {1, 2, 3},
        .addr = {.city = "sh", .zip = 200000},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_TRUE(::flatbuffers::BufferHasIdentifier(encoded->data(), "EVTO"));

    auto root = table_view<person>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    EXPECT_EQ(root[&person::id], 7);
    EXPECT_EQ(root[&person::name], "alice");

    const auto pos = root[&person::pos];
    EXPECT_EQ(pos.x, 10);
    EXPECT_EQ(pos.y, 20);

    auto scores = root[&person::scores];
    ASSERT_TRUE(scores.valid());
    ASSERT_EQ(scores.size(), 3U);
    EXPECT_EQ(scores[0], 1);
    EXPECT_EQ(scores[1], 2);
    EXPECT_EQ(scores[2], 3);

    auto addr = root[&person::addr];
    ASSERT_TRUE(addr.valid());
    EXPECT_EQ(addr[&address::city], "sh");
    EXPECT_EQ(addr[&address::zip], 200000);
}

TEST_CASE(char_and_byte_fields_keep_struct_inline) {
    // char and std::byte are scalars to the fbs backend (proxy.h is_scalar_v),
    // so a trivial struct containing them stays an inline FlatBuffers struct
    // instead of degrading to a table.
    struct probe {
        char tag;
        std::byte flags;
        std::int32_t count;

        auto operator==(const probe&) const -> bool = default;
    };

    static_assert(fbs::is_schema_struct_v<probe>);

    struct frame {
        std::int32_t id;
        probe p;

        auto operator==(const frame&) const -> bool = default;
    };

    const frame input{
        .id = 9,
        .p = {.tag = 'k', .flags = std::byte{0x5A}, .count = 3},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<frame>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    const auto p = root[&frame::p];
    EXPECT_EQ(p.tag, 'k');
    EXPECT_TRUE(p.flags == std::byte{0x5A});
    EXPECT_EQ(p.count, 3);

    frame output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(non_trivial_nested_object_serializes_as_table_offset) {
    const person input{
        .id = 1,
        .name = "n",
        .pos = {.x = 1, .y = 2},
        .scores = {},
        .addr = {.city = "tokyo", .zip = 100},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<person>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto addr = root[&person::addr];
    ASSERT_TRUE(addr.valid());
    EXPECT_EQ(addr[&address::city], "tokyo");
    EXPECT_EQ(addr[&address::zip], 100);
}

TEST_CASE(skip_attr_keeps_field_index_layout) {
    with_skip input{};
    input.a = 3;
    input.internal = 999;
    input.c = 5;

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_skip>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    EXPECT_EQ(root[&with_skip::a], 3);
    EXPECT_FALSE(root.has(&with_skip::internal));
    EXPECT_EQ(root[&with_skip::c], 5);
}

TEST_CASE(vector_of_trivial_struct_serializes_as_struct_vector) {
    struct route {
        std::vector<point> points;
    };

    const route input{
        .points = {{.x = 1, .y = 2}, {.x = 3, .y = 4}},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<route>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto points = root[&route::points];
    ASSERT_TRUE(points.valid());
    ASSERT_EQ(points.size(), 2U);

    const auto p0 = points[0];
    const auto p1 = points[1];
    EXPECT_EQ(p0.x, 1);
    EXPECT_EQ(p0.y, 2);
    EXPECT_EQ(p1.x, 3);
    EXPECT_EQ(p1.y, 4);
}

TEST_CASE(root_vector_preserves_scalar_vector_encoding) {
    const std::vector<std::int32_t> input{3, 5, 8};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    const auto* root = ::flatbuffers::GetRoot<::flatbuffers::Table>(encoded->data());
    ASSERT_TRUE(root != nullptr);

    const auto* vec = root->GetPointer<const ::flatbuffers::Vector<std::int32_t>*>(4);
    ASSERT_TRUE(vec != nullptr);
    ASSERT_EQ(vec->size(), 3U);
    EXPECT_EQ(vec->Get(0), 3);
    EXPECT_EQ(vec->Get(1), 5);
    EXPECT_EQ(vec->Get(2), 8);
}

// --- Optional / smart pointer test structs ---

struct with_optional_scalar {
    std::optional<std::int32_t> value;
};

struct with_optional_string {
    std::optional<std::string> value;
};

struct with_optional_struct {
    std::optional<address> addr;
};

struct with_unique_ptr {
    std::unique_ptr<address> addr;
};

struct with_shared_ptr {
    std::shared_ptr<address> addr;
};

// --- Variant test structs ---

struct with_variant {
    std::variant<std::int32_t, std::string> value;
};

struct with_variant_struct {
    std::variant<std::int32_t, address> value;
};

struct with_vector_of_variants {
    std::vector<std::variant<std::int32_t, std::string>> items;
};

// --- Tuple / pair test structs ---

struct with_pair {
    std::pair<std::int32_t, std::string> value;
};

struct with_tuple {
    std::tuple<std::int32_t, std::string, double> value;
};

struct with_pair_struct_value {
    std::pair<std::string, address> value;
};

struct with_vector_of_pairs {
    std::vector<std::pair<std::int32_t, std::string>> items;
};

// --- Map test structs ---

struct with_map_string_int {
    std::map<std::string, std::int32_t> data;
};

struct with_map_int_string {
    std::map<std::int32_t, std::string> data;
};

struct with_enum_map {
    std::unordered_map<color, std::int32_t> data;
};

struct with_u64_map {
    std::unordered_map<std::uint64_t, std::int32_t> data;
};

struct with_long_doubles {
    std::vector<long double> samples;
    long double scale = 1.0L;
};

struct with_map_string_struct {
    std::map<std::string, address> data;
};

struct with_empty_map {
    std::map<std::string, std::int32_t> data;
};

// --- Edge case test structs ---

struct with_enum {
    color c;
};

struct with_bool {
    bool flag;
};

struct with_vector_strings {
    std::vector<std::string> items;
};

struct with_vector_tables {
    std::vector<address> items;
};

struct with_byte_blobs {
    std::vector<std::vector<std::byte>> blobs;

    auto operator==(const with_byte_blobs&) const -> bool = default;
};

struct with_optional_elements {
    std::vector<std::optional<std::int32_t>> vals;

    auto operator==(const with_optional_elements&) const -> bool = default;
};

struct outer {
    address inner;
    std::vector<std::int32_t> nums;
};

struct deeply_nested_struct {
    outer nested;
};

// ======== Optional / smart pointer tests ========

TEST_CASE(optional_scalar_field_present) {
    with_optional_scalar input{.value = 42};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_optional_scalar>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    ASSERT_TRUE(root.has(&with_optional_scalar::value));
    EXPECT_EQ(root[&with_optional_scalar::value], 42);
}

TEST_CASE(optional_scalar_field_absent) {
    with_optional_scalar input{.value = std::nullopt};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_optional_scalar>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_FALSE(root.has(&with_optional_scalar::value));
    EXPECT_EQ(root[&with_optional_scalar::value], 0);
}

TEST_CASE(optional_string_field) {
    // Present case
    {
        with_optional_string input{.value = "hello"};

        auto encoded = to_bytes(input);
        ASSERT_TRUE(encoded.has_value());

        auto root = table_view<with_optional_string>::from_bytes(*encoded);
        ASSERT_TRUE(root.valid());
        ASSERT_TRUE(root.has(&with_optional_string::value));
        EXPECT_EQ(root[&with_optional_string::value], "hello");
    }

    // Absent case
    {
        with_optional_string input{.value = std::nullopt};

        auto encoded = to_bytes(input);
        ASSERT_TRUE(encoded.has_value());

        auto root = table_view<with_optional_string>::from_bytes(*encoded);
        ASSERT_TRUE(root.valid());
        EXPECT_FALSE(root.has(&with_optional_string::value));
        EXPECT_EQ(root[&with_optional_string::value], "");
    }
}

TEST_CASE(optional_struct_field) {
    // Present case
    {
        with_optional_struct input{
            .addr = address{.city = "paris", .zip = 75000}
        };

        auto encoded = to_bytes(input);
        ASSERT_TRUE(encoded.has_value());

        auto root = table_view<with_optional_struct>::from_bytes(*encoded);
        ASSERT_TRUE(root.valid());
        ASSERT_TRUE(root.has(&with_optional_struct::addr));

        auto addr = root[&with_optional_struct::addr];
        ASSERT_TRUE(addr.valid());
        EXPECT_EQ(addr[&address::city], "paris");
        EXPECT_EQ(addr[&address::zip], 75000);
    }

    // Absent case
    {
        with_optional_struct input{.addr = std::nullopt};

        auto encoded = to_bytes(input);
        ASSERT_TRUE(encoded.has_value());

        auto root = table_view<with_optional_struct>::from_bytes(*encoded);
        ASSERT_TRUE(root.valid());
        EXPECT_FALSE(root.has(&with_optional_struct::addr));

        auto addr = root[&with_optional_struct::addr];
        EXPECT_FALSE(addr.valid());
    }
}

TEST_CASE(unique_ptr_field_present) {
    with_unique_ptr input{};
    input.addr = std::make_unique<address>(address{.city = "berlin", .zip = 10115});

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_unique_ptr>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    ASSERT_TRUE(root.has(&with_unique_ptr::addr));

    auto addr = root[&with_unique_ptr::addr];
    ASSERT_TRUE(addr.valid());
    EXPECT_EQ(addr[&address::city], "berlin");
    EXPECT_EQ(addr[&address::zip], 10115);
}

TEST_CASE(unique_ptr_field_null) {
    with_unique_ptr input{};
    input.addr = nullptr;

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_unique_ptr>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_FALSE(root.has(&with_unique_ptr::addr));

    auto addr = root[&with_unique_ptr::addr];
    EXPECT_FALSE(addr.valid());
}

TEST_CASE(shared_ptr_field) {
    // Present case
    {
        with_shared_ptr input{};
        input.addr = std::make_shared<address>(address{.city = "london", .zip = 20000});

        auto encoded = to_bytes(input);
        ASSERT_TRUE(encoded.has_value());

        auto root = table_view<with_shared_ptr>::from_bytes(*encoded);
        ASSERT_TRUE(root.valid());
        ASSERT_TRUE(root.has(&with_shared_ptr::addr));

        auto addr = root[&with_shared_ptr::addr];
        ASSERT_TRUE(addr.valid());
        EXPECT_EQ(addr[&address::city], "london");
        EXPECT_EQ(addr[&address::zip], 20000);
    }

    // Null case
    {
        with_shared_ptr input{};
        input.addr = nullptr;

        auto encoded = to_bytes(input);
        ASSERT_TRUE(encoded.has_value());

        auto root = table_view<with_shared_ptr>::from_bytes(*encoded);
        ASSERT_TRUE(root.valid());
        EXPECT_FALSE(root.has(&with_shared_ptr::addr));
    }
}

// ======== Variant tests ========

TEST_CASE(variant_scalar_alternative) {
    with_variant input{.value = std::int32_t{99}};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_variant>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto v = root[&with_variant::value];
    ASSERT_TRUE(v.valid());
    EXPECT_EQ(v.index(), 0U);
    EXPECT_EQ(v.get<0>(), 99);
}

TEST_CASE(variant_string_alternative) {
    with_variant input{.value = std::string("kotatsu")};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_variant>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto v = root[&with_variant::value];
    ASSERT_TRUE(v.valid());
    EXPECT_EQ(v.index(), 1U);
    EXPECT_EQ(v.get<1>(), "kotatsu");
}

TEST_CASE(variant_struct_alternative) {
    with_variant_struct input{
        .value = address{.city = "rome", .zip = 100}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_variant_struct>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto v = root[&with_variant_struct::value];
    ASSERT_TRUE(v.valid());
    EXPECT_EQ(v.index(), 1U);

    auto addr = v.get<1>();
    ASSERT_TRUE(addr.valid());
    EXPECT_EQ(addr[&address::city], "rome");
    EXPECT_EQ(addr[&address::zip], 100);
}

TEST_CASE(vector_of_variants) {
    // Variant elements travel as their variant table directly in the vector
    // (no per-element wrapper): encode, decode and the lazy view agree.
    with_vector_of_variants input{
        .items = {std::int32_t{7}, std::string("kotatsu"), std::int32_t{9}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    with_vector_of_variants output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output.items, input.items);

    auto root = table_view<with_vector_of_variants>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto items = root[&with_vector_of_variants::items];
    ASSERT_TRUE(items.valid());
    ASSERT_EQ(items.size(), 3U);

    auto v0 = items[0];
    ASSERT_TRUE(v0.valid());
    EXPECT_EQ(v0.index(), 0U);
    EXPECT_EQ(v0.get<0>(), 7);

    auto v1 = items[1];
    ASSERT_TRUE(v1.valid());
    EXPECT_EQ(v1.index(), 1U);
    EXPECT_EQ(v1.get<1>(), "kotatsu");
}

// ======== Tuple / pair tests ========

TEST_CASE(pair_field) {
    with_pair input{
        .value = {42, "hello"}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_pair>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto p = root[&with_pair::value];
    ASSERT_TRUE(p.valid());
    EXPECT_EQ(p.get<0>(), 42);
    EXPECT_EQ(p.get<1>(), "hello");
}

TEST_CASE(tuple_field) {
    with_tuple input{
        .value = {7, "world", 3.14}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_tuple>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto t = root[&with_tuple::value];
    ASSERT_TRUE(t.valid());
    EXPECT_EQ(t.get<0>(), 7);
    EXPECT_EQ(t.get<1>(), "world");
    EXPECT_EQ(t.get<2>(), 3.14);
}

TEST_CASE(pair_with_struct_value) {
    with_pair_struct_value input{
        .value = {"key", address{.city = "nyc", .zip = 10001}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_pair_struct_value>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto p = root[&with_pair_struct_value::value];
    ASSERT_TRUE(p.valid());
    EXPECT_EQ(p.get<0>(), "key");

    auto addr = p.get<1>();
    ASSERT_TRUE(addr.valid());
    EXPECT_EQ(addr[&address::city], "nyc");
    EXPECT_EQ(addr[&address::zip], 10001);
}

TEST_CASE(vector_of_pairs) {
    with_vector_of_pairs input{
        .items = {{1, "a"}, {2, "b"}, {3, "c"}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_vector_of_pairs>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto items = root[&with_vector_of_pairs::items];
    ASSERT_TRUE(items.valid());
    ASSERT_EQ(items.size(), 3U);

    auto e0 = items[0];
    EXPECT_EQ(e0.get<0>(), 1);
    EXPECT_EQ(e0.get<1>(), "a");

    auto e2 = items[2];
    EXPECT_EQ(e2.get<0>(), 3);
    EXPECT_EQ(e2.get<1>(), "c");
}

// ======== Map tests ========

TEST_CASE(map_string_to_int) {
    with_map_string_int input{
        .data = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_string_int>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_string_int::data];
    ASSERT_TRUE(m.valid());
    ASSERT_EQ(m.size(), 3U);

    // std::map is ordered, so entries are alpha, beta, gamma
    auto e0 = m.at(0);
    ASSERT_TRUE(e0.valid());
    EXPECT_EQ(e0.get<0>(), "alpha");
    EXPECT_EQ(e0.get<1>(), 1);

    auto e1 = m.at(1);
    EXPECT_EQ(e1.get<0>(), "beta");
    EXPECT_EQ(e1.get<1>(), 2);

    auto e2 = m.at(2);
    EXPECT_EQ(e2.get<0>(), "gamma");
    EXPECT_EQ(e2.get<1>(), 3);
}

TEST_CASE(map_int_to_string) {
    with_map_int_string input{
        .data = {{10, "ten"}, {20, "twenty"}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_int_string>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_int_string::data];
    ASSERT_TRUE(m.valid());
    ASSERT_EQ(m.size(), 2U);

    auto e0 = m.at(0);
    EXPECT_EQ(e0.get<0>(), 10);
    EXPECT_EQ(e0.get<1>(), "ten");

    auto e1 = m.at(1);
    EXPECT_EQ(e1.get<0>(), 20);
    EXPECT_EQ(e1.get<1>(), "twenty");
}

TEST_CASE(map_string_to_struct) {
    with_map_string_struct input{
        .data = {{"home", address{.city = "sf", .zip = 94102}},
                 {"work", address{.city = "la", .zip = 90001}}},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_string_struct>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_string_struct::data];
    ASSERT_TRUE(m.valid());
    ASSERT_EQ(m.size(), 2U);

    // std::map ordered: "home" < "work"
    auto e0 = m.at(0);
    EXPECT_EQ(e0.get<0>(), "home");
    auto addr0 = e0.get<1>();
    ASSERT_TRUE(addr0.valid());
    EXPECT_EQ(addr0[&address::city], "sf");
    EXPECT_EQ(addr0[&address::zip], 94102);

    auto e1 = m.at(1);
    EXPECT_EQ(e1.get<0>(), "work");
    auto addr1 = e1.get<1>();
    ASSERT_TRUE(addr1.valid());
    EXPECT_EQ(addr1[&address::city], "la");
    EXPECT_EQ(addr1[&address::zip], 90001);
}

TEST_CASE(empty_map) {
    with_empty_map input{.data = {}};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_empty_map>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_empty_map::data];
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0U);
}

TEST_CASE(map_key_lookup_string_key) {
    with_map_string_int input{
        .data = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_string_int>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_string_int::data];
    ASSERT_TRUE(m.valid());

    EXPECT_EQ(m[std::string("alpha")], 1);
    EXPECT_EQ(m[std::string("beta")], 2);
    EXPECT_EQ(m[std::string("gamma")], 3);
    // Missing key returns default
    EXPECT_EQ(m[std::string("missing")], 0);
}

TEST_CASE(map_key_lookup_int_key) {
    // 2 and 10 order differently as numbers and as decimal strings: the
    // encoder must sort entries by the same numeric order the lazy lookup's
    // binary search compares by.
    with_map_int_string input{
        .data = {{-7, "minus"}, {2, "two"}, {10, "ten"}, {30, "thirty"}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_int_string>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_int_string::data];
    ASSERT_TRUE(m.valid());

    EXPECT_EQ(m[-7], "minus");
    EXPECT_EQ(m[2], "two");
    EXPECT_EQ(m[10], "ten");
    EXPECT_EQ(m[30], "thirty");
    // Missing key returns default (empty string_view)
    EXPECT_EQ(m[99], "");
}

TEST_CASE(map_key_lookup_enum_key) {
    // Enum keys sort by their underlying value on encode; the lazy lookup's
    // binary search compares decoded enums, which order the same way. The
    // unordered input forces the encoder's sort to do real work.
    with_enum_map input{
        .data = {{color::blue, 3}, {color::red, 1}, {color::green, 2}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_enum_map>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_enum_map::data];
    ASSERT_TRUE(m.valid());
    EXPECT_EQ(m[color::red], 1);
    EXPECT_EQ(m[color::green], 2);
    EXPECT_EQ(m[color::blue], 3);
}

TEST_CASE(vector_of_long_double_roundtrips) {
    // long double stores as double cells (scalar_cell_t) everywhere: the
    // vector fast path must not memcpy 16-byte long doubles the decode side
    // reads as 8-byte cells, and fields must not AddElement 16-byte cells.
    with_long_doubles input{
        .samples = {1.5L, -2.25L, 1024.0L},
        .scale = 3.5L,
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    with_long_doubles output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    ASSERT_EQ(output.samples.size(), 3U);
    EXPECT_EQ(output.samples[0], 1.5L);
    EXPECT_EQ(output.samples[1], -2.25L);
    EXPECT_EQ(output.samples[2], 1024.0L);
    EXPECT_EQ(output.scale, 3.5L);

    // The lazy view reads the same double cells.
    auto root = table_view<with_long_doubles>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&with_long_doubles::scale], 3.5L);
    auto arr = root[&with_long_doubles::samples];
    ASSERT_EQ(arr.size(), 3U);
    EXPECT_EQ(arr[1], -2.25L);
}

TEST_CASE(map_key_lookup_uint64_key) {
    // Keys above INT64_MAX must keep unsigned ordering end to end; a signed
    // ordering key would sort them first and break the binary search.
    with_u64_map input{
        .data = {{2U, 1}, {0x8000000000000001ULL, 2}, {42U, 3}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_u64_map>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_u64_map::data];
    ASSERT_TRUE(m.valid());
    EXPECT_EQ(m[2U], 1);
    EXPECT_EQ(m[42U], 3);
    EXPECT_EQ(m[0x8000000000000001ULL], 2);
}

TEST_CASE(vector_of_trivial_struct_roundtrips_eagerly) {
    // Inline-struct elements decode through ScalarReader::visit_struct; the
    // list container also exercises the element-wise collector on encode (no
    // contiguous fast path).
    struct route {
        std::vector<point> points;
        std::list<point> waypoints;
    };

    const route input{
        .points = {{.x = 1, .y = 2}, {.x = 3, .y = 4}},
        .waypoints = {{.x = 5, .y = 6}, {.x = 7, .y = 8}},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    route output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    ASSERT_EQ(output.points.size(), 2U);
    EXPECT_EQ(output.points[1].x, 3);
    EXPECT_EQ(output.points[1].y, 4);
    ASSERT_EQ(output.waypoints.size(), 2U);
    EXPECT_EQ(output.waypoints.back().x, 7);
    EXPECT_EQ(output.waypoints.back().y, 8);
}

TEST_CASE(map_find_existing) {
    with_map_string_int input{
        .data = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_string_int>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_string_int::data];
    ASSERT_TRUE(m.valid());

    auto result = m.find(std::string("beta"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get<0>(), "beta");
    EXPECT_EQ(result->get<1>(), 2);
}

TEST_CASE(map_find_missing) {
    with_map_string_int input{
        .data = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_string_int>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_string_int::data];
    ASSERT_TRUE(m.valid());

    auto result = m.find(std::string("missing"));
    EXPECT_FALSE(result.has_value());
}

TEST_CASE(map_contains) {
    with_map_string_int input{
        .data = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_string_int>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto m = root[&with_map_string_int::data];
    ASSERT_TRUE(m.valid());

    EXPECT_TRUE(m.contains(std::string("alpha")));
    EXPECT_TRUE(m.contains(std::string("beta")));
    EXPECT_TRUE(m.contains(std::string("gamma")));
    EXPECT_FALSE(m.contains(std::string("missing")));
}

TEST_CASE(map_transparent_lookup) {
    with_map_string_int input{
        .data = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_map_string_int>::from_bytes(*encoded);
    auto m = root[&with_map_string_int::data];
    ASSERT_TRUE(m.valid());

    // lookup with const char*
    EXPECT_EQ(m["beta"], 2);
    EXPECT_TRUE(m.contains("alpha"));
    EXPECT_FALSE(m.contains("missing"));

    // lookup with string_view
    std::string_view sv = "gamma";
    EXPECT_EQ(m[sv], 3);
    EXPECT_TRUE(m.contains(sv));

    auto result = m.find("beta");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->template get<0>(), "beta");
    EXPECT_EQ(result->template get<1>(), 2);

    auto missing = m.find("nope");
    EXPECT_FALSE(missing.has_value());
}

// ======== Edge case tests ========

TEST_CASE(enum_field) {
    with_enum input{.c = color::blue};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_enum>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&with_enum::c], color::blue);
}

TEST_CASE(bool_field) {
    with_bool input{.flag = true};

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_bool>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&with_bool::flag], true);
}

TEST_CASE(vector_of_strings) {
    with_vector_strings input{
        .items = {"hello", "world", "test"}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_vector_strings>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto items = root[&with_vector_strings::items];
    ASSERT_TRUE(items.valid());
    ASSERT_EQ(items.size(), 3U);
    EXPECT_EQ(items[0], "hello");
    EXPECT_EQ(items[1], "world");
    EXPECT_EQ(items[2], "test");
}

TEST_CASE(vector_of_nested_tables) {
    with_vector_tables input{
        .items = {address{.city = "a", .zip = 1}, address{.city = "b", .zip = 2}},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<with_vector_tables>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto items = root[&with_vector_tables::items];
    ASSERT_TRUE(items.valid());
    ASSERT_EQ(items.size(), 2U);

    auto a0 = items[0];
    ASSERT_TRUE(a0.valid());
    EXPECT_EQ(a0[&address::city], "a");
    EXPECT_EQ(a0[&address::zip], 1);

    auto a1 = items[1];
    ASSERT_TRUE(a1.valid());
    EXPECT_EQ(a1[&address::city], "b");
    EXPECT_EQ(a1[&address::zip], 2);
}

TEST_CASE(deeply_nested) {
    deeply_nested_struct input{
        .nested = {.inner = {.city = "deep", .zip = 999}, .nums = {10, 20, 30}},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<deeply_nested_struct>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto nested = root[&deeply_nested_struct::nested];
    ASSERT_TRUE(nested.valid());

    auto inner = nested[&outer::inner];
    ASSERT_TRUE(inner.valid());
    EXPECT_EQ(inner[&address::city], "deep");
    EXPECT_EQ(inner[&address::zip], 999);

    auto nums = nested[&outer::nums];
    ASSERT_TRUE(nums.valid());
    ASSERT_EQ(nums.size(), 3U);
    EXPECT_EQ(nums[0], 10);
    EXPECT_EQ(nums[1], 20);
    EXPECT_EQ(nums[2], 30);
}

TEST_CASE(vector_of_byte_blobs) {
    // Byte blobs have no direct vector-of-vectors representation: each
    // element travels boxed in a wrapper table with the byte vector at its
    // first field, in the encoded bytes and in the lazy view.
    with_byte_blobs input{
        .blobs = {{std::byte{0xAA}, std::byte{0xBB}}, {}, {std::byte{0x01}}},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    with_byte_blobs output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output, input);

    auto root = table_view<with_byte_blobs>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto blobs = root[&with_byte_blobs::blobs];
    ASSERT_TRUE(blobs.valid());
    ASSERT_EQ(blobs.size(), 3U);

    auto b0 = blobs[0];
    ASSERT_TRUE(b0.valid());
    ASSERT_EQ(b0.size(), 2U);
    EXPECT_EQ(b0[0], std::byte{0xAA});
    EXPECT_EQ(b0[1], std::byte{0xBB});
    EXPECT_EQ(blobs[1].size(), 0U);
}

TEST_CASE(vector_of_optional_scalars) {
    // Nullable elements are boxed per element; the lazy view reads through
    // the wrapper table and peels absence to the element default.
    with_optional_elements input{
        .vals = {5, std::nullopt, 9}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    with_optional_elements output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output, input);

    auto root = table_view<with_optional_elements>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto vals = root[&with_optional_elements::vals];
    ASSERT_TRUE(vals.valid());
    ASSERT_EQ(vals.size(), 3U);
    EXPECT_EQ(vals[0], 5);
    EXPECT_EQ(vals[1], 0);  // absent element reads as the scalar default
    EXPECT_EQ(vals[2], 9);
}

TEST_CASE(empty_from_bytes) {
    std::vector<std::uint8_t> empty_data{};
    auto root = table_view<person>::from_bytes(std::span<const std::uint8_t>(empty_data));
    EXPECT_FALSE(root.valid());
}

TEST_CASE(array_view_out_of_bounds) {
    person input{
        .id = 1,
        .name = "n",
        .pos = {.x = 0, .y = 0},
        .scores = {100},
        .addr = {.city = "x", .zip = 0},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<person>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto scores = root[&person::scores];
    ASSERT_TRUE(scores.valid());
    ASSERT_EQ(scores.size(), 1U);

    // Out-of-bounds access should return default
    EXPECT_EQ(scores[1], 0);
    EXPECT_EQ(scores[100], 0);
}

TEST_CASE(roundtrip_nested_struct) {
    const person input{
        .id = 7,
        .name = "alice",
        .pos = {.x = 10, .y = 20},
        .scores = {1, 2, 3},
        .addr = {.city = "sh", .zip = 200000},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    person output{};
    auto status = fbs::from_bytes(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(roundtrip_root_vector_and_variant) {
    const std::vector<std::int32_t> input_vec{3, 5, 8};
    auto encoded_vec = to_bytes(input_vec);
    ASSERT_TRUE(encoded_vec.has_value());

    std::vector<std::int32_t> output_vec{};
    auto vec_status = fbs::from_bytes(*encoded_vec, output_vec);
    ASSERT_TRUE(vec_status.has_value());
    EXPECT_EQ(output_vec, input_vec);

    using sample_variant = std::variant<std::int32_t, std::string>;
    const sample_variant input_var = std::string("kotatsu");

    auto encoded_var = to_bytes(input_var);
    ASSERT_TRUE(encoded_var.has_value());

    sample_variant output_var = std::int32_t{0};
    auto var_status = fbs::from_bytes(*encoded_var, output_var);
    ASSERT_TRUE(var_status.has_value());
    EXPECT_EQ(output_var, input_var);
}

// --- Inline structs with default member initializers, struct map keys ---

/// Mirrors an index occurrence: nested memcpy-image structs whose sentinel
/// defaults break std::is_trivial but not trivial copyability.
struct sentinel_range {
    std::uint32_t begin = static_cast<std::uint32_t>(-1);
    std::uint32_t end = static_cast<std::uint32_t>(-1);

    friend bool operator==(const sentinel_range&, const sentinel_range&) = default;
};

/// The signed field pins the ordering to field values: -2 sorts before 3
/// field-wise, after it byte-wise.
struct occurrence_key {
    sentinel_range range;
    std::uint64_t target = 0;
    std::int32_t weight = 0;

    friend bool operator==(const occurrence_key&, const occurrence_key&) = default;
};

static_assert(fbs::can_inline_struct_v<sentinel_range>);
static_assert(fbs::can_inline_struct_v<occurrence_key>);
// A string member keeps a struct table-shaped even under the relaxed
// trivially-copyable bound.
static_assert(!fbs::can_inline_struct_v<address>);

/// std::optional<std::int32_t> passes the trivially-copyable bound, so only
/// the explicit optional exclusion keeps this struct table-shaped; a memcpy
/// image would let tampered buffers forge the engagement flag.
struct with_optional_member {
    std::optional<std::int32_t> cached;
    std::int32_t id = 0;

    friend bool operator==(const with_optional_member&, const with_optional_member&) = default;
};

static_assert(std::is_trivially_copyable_v<with_optional_member>);
static_assert(!fbs::can_inline_struct_v<with_optional_member>);

TEST_CASE(optional_bearing_struct_stays_table_shaped) {
    struct holder {
        std::vector<with_optional_member> entries;
    };

    const holder input{
        .entries = {{.cached = 7, .id = 1}, {.cached = std::nullopt, .id = 2}}
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    holder output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output.entries, input.entries);
}

/// long double lowers to a double cell everywhere else in this backend; its
/// native image is ABI-specific and can carry internal padding (x86's 80-bit
/// format leaves 6 of 16 bytes unspecified), so a memcpy image would
/// disclose those bytes. A struct holding one stays table-shaped.
struct with_long_double_member {
    long double ratio = 0.0L;
    std::int32_t id = 0;

    friend bool operator==(const with_long_double_member&,
                           const with_long_double_member&) = default;
};

static_assert(std::is_trivially_copyable_v<with_long_double_member>);
static_assert(!fbs::can_inline_struct_v<with_long_double_member>);

TEST_CASE(long_double_bearing_struct_stays_table_shaped) {
    struct holder {
        std::vector<with_long_double_member> entries;
        with_long_double_member solo;
    };

    const holder input{
        .entries = {{.ratio = 2.5L, .id = 1}, {.ratio = -0.5L, .id = 2}},
        .solo = {.ratio = 0.25L,           .id = 3                  },
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    holder output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output.entries, input.entries);
    EXPECT_EQ(output.solo, input.solo);
}

/// One past the reflection field limit: meta::field_count() collapses to
/// zero, indistinguishable from an empty struct, so the padding
/// sanitization would zero the whole image. Such a struct — and anything
/// containing one — must stay table-shaped.
struct over_limit_key {
    std::int32_t f00, f01, f02, f03, f04, f05, f06, f07, f08, f09, f10, f11, f12, f13, f14, f15,
        f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33,
        f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51,
        f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69,
        f70, f71, f72;
};

static_assert(std::is_trivially_copyable_v<over_limit_key> &&
              std::is_standard_layout_v<over_limit_key>);
static_assert(meta::field_count<over_limit_key>() == 0);
static_assert(!fbs::can_inline_struct_v<over_limit_key>);

struct holds_over_limit {
    over_limit_key wide;
};

static_assert(!fbs::can_inline_struct_v<holds_over_limit>);

/// Trivially copyable admits deleted assignment operators, but decode
/// restores an inline struct by whole-object assignment, so a
/// non-assignable struct must stay table-shaped, where its fields decode
/// individually.
struct pinned_key {
    char tag = 0;
    std::int32_t id = 0;

    pinned_key& operator=(const pinned_key&) = delete;

    friend bool operator==(const pinned_key&, const pinned_key&) = default;
};

static_assert(std::is_trivially_copyable_v<pinned_key>);
static_assert(!std::is_copy_assignable_v<pinned_key>);
static_assert(!fbs::can_inline_struct_v<pinned_key>);

TEST_CASE(non_assignable_struct_decodes_field_by_field) {
    // A solo field only: copying a pinned_key is deprecated (user-declared
    // copy assignment), so the roundtrip must never need the whole object —
    // exactly what the table shape provides.
    struct holder {
        pinned_key solo;
        std::int32_t tail = 0;
    };

    const holder input{
        .solo = {.tag = 's', .id = 7},
        .tail = 3
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    holder output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output.solo, input.solo);
    EXPECT_EQ(output.tail, input.tail);
}

/// char followed by int: three native padding bytes whose content must
/// never reach the buffer.
struct padded_key {
    char tag = 0;
    std::int32_t id = 0;

    friend bool operator==(const padded_key&, const padded_key&) = default;
};

static_assert(fbs::can_inline_struct_v<padded_key>);
static_assert(sizeof(padded_key) > sizeof(std::int32_t) + sizeof(char));

struct padded_key_less {
    bool operator()(const padded_key& a, const padded_key& b) const {
        return std::tie(a.tag, a.id) < std::tie(b.tag, b.id);
    }
};

struct with_padded_structs {
    std::vector<padded_key> items;
    padded_key solo;
    std::map<padded_key, std::int32_t, padded_key_less> scores;
};

/// A padded_key whose padding bytes hold fill before the fields are set.
/// memcpy rather than memset: padded_key's default member initializers make
/// it non-trivial, which GCC's -Wclass-memaccess rejects for memset, while
/// memcpy into a trivially-copyable object is the sanctioned spelling and
/// guarantees the exact object representation, padding included.
auto scribbled(std::uint8_t fill, char tag, std::int32_t id) -> padded_key {
    std::array<std::byte, sizeof(padded_key)> bytes;
    bytes.fill(std::byte{fill});
    padded_key k;
    std::memcpy(&k, bytes.data(), sizeof(k));
    k.tag = tag;
    k.id = id;
    return k;
}

/// Overwrite a stored padded_key's bytes with fill, then splice the field
/// values back — written directly into the final storage, so no later copy
/// can legally drop the padding fill on the way to the encoder.
void scribble_stored(padded_key& stored, std::uint8_t fill) {
    const padded_key value = stored;
    std::array<std::byte, sizeof(padded_key)> bytes;
    bytes.fill(std::byte{fill});
    std::memcpy(bytes.data() + offsetof(padded_key, tag), &value.tag, sizeof(value.tag));
    std::memcpy(bytes.data() + offsetof(padded_key, id), &value.id, sizeof(value.id));
    std::memcpy(&stored, bytes.data(), sizeof(stored));
}

TEST_CASE(inline_struct_padding_never_reaches_the_buffer) {
    // The same logical value with two different paddings, covering every
    // inline-struct write path: table field, vector element, map key.
    with_padded_structs noisy;
    noisy.solo = scribbled(0xFF, 'x', 7);
    noisy.items.push_back(scribbled(0xFF, 'y', 9));
    noisy.scores.emplace(scribbled(0xFF, 'k', 3), 1);

    // Copies into storage may legally drop the fill (optimized builds do);
    // re-scribble the reachable stored objects in place so the noisy side
    // provably carries dirty padding into the encoder. The map key is const
    // in storage and cannot be re-scribbled — the direct buffer assertions
    // below cover its path deterministically instead.
    scribble_stored(noisy.solo, 0xFF);
    scribble_stored(noisy.items[0], 0xFF);

    with_padded_structs quiet;
    quiet.solo = scribbled(0x00, 'x', 7);
    quiet.items.push_back(scribbled(0x00, 'y', 9));
    quiet.scores.emplace(scribbled(0x00, 'k', 3), 1);

    auto noisy_bytes = to_bytes(noisy);
    auto quiet_bytes = to_bytes(quiet);
    ASSERT_TRUE(noisy_bytes.has_value());
    ASSERT_TRUE(quiet_bytes.has_value());

    // The documented wire property, asserted directly on the buffer: the
    // stored inline structs' padding bytes encode as zero whatever the
    // source objects held. Slots follow declaration order (first_field +
    // 2 * index): items=4, solo=6, scores=8; a map entry holds its key at
    // the entry table's first slot.
    auto padding_is_zero = [](const padded_key* stored) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(stored);
        for(std::size_t i = sizeof(char); i < offsetof(padded_key, id); ++i) {
            if(bytes[i] != 0) {
                return false;
            }
        }
        return true;
    };

    const auto* root = ::flatbuffers::GetRoot<::flatbuffers::Table>(noisy_bytes->data());
    const auto* solo = root->GetStruct<const padded_key*>(6);
    ASSERT_TRUE(solo != nullptr);
    EXPECT_TRUE(padding_is_zero(solo));

    const auto* items = root->GetPointer<const ::flatbuffers::Vector<const padded_key*>*>(4);
    ASSERT_TRUE(items != nullptr);
    EXPECT_TRUE(padding_is_zero(items->Get(0)));

    const auto* entries =
        root->GetPointer<const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::Table>>*>(
            8);
    ASSERT_TRUE(entries != nullptr);
    const auto* key = entries->Get(0)->GetStruct<const padded_key*>(4);
    ASSERT_TRUE(key != nullptr);
    EXPECT_TRUE(padding_is_zero(key));

    // Anything short of byte-identical output would disclose the memory
    // previously occupying the padding and break encode determinism.
    EXPECT_EQ(*noisy_bytes, *quiet_bytes);

    with_padded_structs output{};
    ASSERT_TRUE(fbs::from_bytes(*noisy_bytes, output).has_value());
    EXPECT_EQ(output.solo, noisy.solo);
    EXPECT_EQ(output.items, noisy.items);
    ASSERT_TRUE(output.scores.contains(padded_key{.tag = 'k', .id = 3}));
    EXPECT_EQ(output.scores.at(padded_key{.tag = 'k', .id = 3}), 1);
}

/// Deliberately the reverse of the reflected field order: the encoder must
/// sort entries by its own canonical ordering, not trust the container's.
struct reverse_key_less {
    bool operator()(const occurrence_key& a, const occurrence_key& b) const {
        return std::tie(b.range.begin, b.range.end, b.target, b.weight) <
               std::tie(a.range.begin, a.range.end, a.target, a.weight);
    }
};

struct occurrence_key_hash {
    std::size_t operator()(const occurrence_key& k) const {
        return std::hash<std::uint64_t>{}(k.target) ^ (k.range.begin * 31U) ^ k.range.end ^
               static_cast<std::uint32_t>(k.weight);
    }
};

struct with_struct_key_map {
    std::map<occurrence_key, std::int32_t, reverse_key_less> data;
};

TEST_CASE(inline_struct_with_default_member_initializers_roundtrips) {
    struct route_log {
        std::vector<occurrence_key> hits;
        occurrence_key last;
    };

    const route_log input{
        .hits = {{.range = {.begin = 1, .end = 5}, .target = 9, .weight = 0},
                 {.range = {.begin = 2, .end = 3}, .target = 7, .weight = -1}},
        .last = {.range = {.begin = 8, .end = 9}, .target = 1, .weight = 2},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    route_log output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    ASSERT_EQ(output.hits.size(), 2U);
    EXPECT_EQ(output.hits[0].range.begin, 1U);
    EXPECT_EQ(output.hits[0].target, 9U);
    EXPECT_EQ(output.hits[1].range.end, 3U);
    EXPECT_EQ(output.last.range.end, 9U);

    auto root = table_view<route_log>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    auto hits = root[&route_log::hits];
    ASSERT_EQ(hits.size(), 2U);
    EXPECT_EQ(hits[0].range.end, 5U);
    EXPECT_EQ(hits[1].range.begin, 2U);
    EXPECT_EQ(root[&route_log::last].target, 1U);
}

TEST_CASE(struct_keyed_map_sorts_by_reflected_order_and_looks_up) {
    // Each adjacent pair is decided by a different field, always against a
    // contrary later field: k1/k2 by the signed weight, k2/k3 by target,
    // k3/k4 by the nested range.end, k4/k5 by range.begin.
    const occurrence_key k1{
        .range = {.begin = 1, .end = 5},
        .target = 9,
        .weight = -2
    };
    const occurrence_key k2{
        .range = {.begin = 1, .end = 5},
        .target = 9,
        .weight = 3
    };
    const occurrence_key k3{
        .range = {.begin = 1, .end = 5},
        .target = 10,
        .weight = -9
    };
    const occurrence_key k4{
        .range = {.begin = 1, .end = 6},
        .target = 0,
        .weight = 0
    };
    const occurrence_key k5{
        .range = {.begin = 2, .end = 0},
        .target = 0,
        .weight = 0
    };

    with_struct_key_map input;
    input.data.emplace(k1, 1);
    input.data.emplace(k2, 2);
    input.data.emplace(k3, 3);
    input.data.emplace(k4, 4);
    input.data.emplace(k5, 5);

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    with_struct_key_map output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output.data, input.data);

    auto root = table_view<with_struct_key_map>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    auto m = root[&with_struct_key_map::data];
    ASSERT_EQ(m.size(), 5U);

    EXPECT_EQ(m[k1], 1);
    EXPECT_EQ(m[k2], 2);
    EXPECT_EQ(m[k3], 3);
    EXPECT_EQ(m[k4], 4);
    EXPECT_EQ(m[k5], 5);

    auto found = m.find(k3);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->get<0>(), k3);
    EXPECT_EQ(found->get<1>(), 3);

    // Misses on every side of the entry range: before the first key,
    // between two entries, after the last key.
    const occurrence_key before{
        .range = {.begin = 0, .end = 0},
        .target = 0,
        .weight = 0
    };
    const occurrence_key between{
        .range = {.begin = 1, .end = 5},
        .target = 9,
        .weight = 0
    };
    const occurrence_key after{
        .range = {.begin = 9, .end = 0},
        .target = 0,
        .weight = 0
    };
    EXPECT_FALSE(m.contains(before));
    EXPECT_FALSE(m.contains(between));
    EXPECT_FALSE(m.contains(after));
    EXPECT_FALSE(m.find(between).has_value());

    // Entries land in field-wise lexicographic order regardless of the
    // container's (reversed) iteration order.
    EXPECT_EQ(m.at(0).get<0>(), k1);
    EXPECT_EQ(m.at(1).get<0>(), k2);
    EXPECT_EQ(m.at(2).get<0>(), k3);
    EXPECT_EQ(m.at(3).get<0>(), k4);
    EXPECT_EQ(m.at(4).get<0>(), k5);
}

TEST_CASE(struct_keyed_unordered_map_encodes_sorted) {
    struct with_struct_key_umap {
        std::unordered_map<occurrence_key, std::int32_t, occurrence_key_hash> data;
    };

    with_struct_key_umap input;
    input.data.emplace(
        occurrence_key{
            .range = {.begin = 2, .end = 1},
            .target = 0,
            .weight = 0
    },
        20);
    input.data.emplace(
        occurrence_key{
            .range = {.begin = 1, .end = 1},
            .target = 0,
            .weight = 0
    },
        10);
    input.data.emplace(
        occurrence_key{
            .range = {.begin = 3, .end = 1},
            .target = 0,
            .weight = 0
    },
        30);

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());

    with_struct_key_umap output{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, output).has_value());
    EXPECT_EQ(output.data, input.data);

    auto root = table_view<with_struct_key_umap>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    auto m = root[&with_struct_key_umap::data];
    ASSERT_EQ(m.size(), 3U);
    EXPECT_EQ(m.at(0).get<1>(), 10);
    EXPECT_EQ(m.at(1).get<1>(), 20);
    EXPECT_EQ(m.at(2).get<1>(), 30);
}

TEST_CASE(from_verified_bytes_wraps_without_reverifying) {
    // The memory-map-once pattern: verify at load via from_bytes, then
    // construct throwaway views per query without paying verification again.
    const person input{
        .id = 7,
        .name = "alice",
        .pos = {.x = 10, .y = 20},
        .scores = {1, 2, 3},
        .addr = {.city = "sh", .zip = 200000},
    };

    auto encoded = to_bytes(input);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_TRUE(table_view<person>::from_bytes(*encoded).valid());

    auto root = table_view<person>::from_verified_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    EXPECT_EQ(root[&person::id], 7);
    EXPECT_EQ(root[&person::name], "alice");
    EXPECT_EQ(root[&person::scores].size(), 3U);

    auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(encoded->data()),
                                            encoded->size());
    EXPECT_EQ(table_view<person>::from_verified_bytes(bytes)[&person::id], 7);
}

TEST_CASE(struct_keyed_map_empty_and_single_entry) {
    const with_struct_key_map empty{};
    auto encoded_empty = to_bytes(empty);
    ASSERT_TRUE(encoded_empty.has_value());

    with_struct_key_map empty_out{};
    ASSERT_TRUE(fbs::from_bytes(*encoded_empty, empty_out).has_value());
    EXPECT_TRUE(empty_out.data.empty());

    auto empty_root = table_view<with_struct_key_map>::from_bytes(*encoded_empty);
    ASSERT_TRUE(empty_root.valid());
    EXPECT_FALSE(empty_root[&with_struct_key_map::data].contains(
        occurrence_key{.range = {}, .target = 1, .weight = 0}));

    const occurrence_key only{
        .range = {.begin = 3, .end = 4},
        .target = 5,
        .weight = 0
    };
    with_struct_key_map single;
    single.data.emplace(only, 42);
    auto encoded = to_bytes(single);
    ASSERT_TRUE(encoded.has_value());

    with_struct_key_map single_out{};
    ASSERT_TRUE(fbs::from_bytes(*encoded, single_out).has_value());
    EXPECT_EQ(single_out.data, single.data);

    auto root = table_view<with_struct_key_map>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    auto m = root[&with_struct_key_map::data];
    EXPECT_EQ(m[only], 42);
    EXPECT_FALSE(m.contains(occurrence_key{
        .range = {.begin = 3, .end = 4},
        .target = 6,
        .weight = 0
    }));
}

};  // TEST_SUITE(serde_flatbuffers_object)

}  // namespace

}  // namespace kota::codec

#endif
