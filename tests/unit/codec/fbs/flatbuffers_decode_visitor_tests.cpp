#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "../standard_case_suite.h"
#include "fixtures/schema/common.h"
#include "kota/zest/zest.h"
#include "kota/meta/attrs.h"
#include "kota/codec/fbs/decode.h"
#include "kota/codec/fbs/encode.h"

namespace kota::codec {

using namespace meta;

namespace {

using fbs::to_flatbuffer;
using fbs::from_flatbuffer;

using point = meta::fixtures::Point2i;
using address = meta::fixtures::Address;

struct person {
    std::int32_t id;
    std::string name;
    point pos;
    std::vector<std::int32_t> scores;
    address addr;
};

template <typename T>
auto roundtrip(const T& input) -> std::expected<T, rich_error> {
    auto encoded = to_flatbuffer(input);
    if(!encoded) {
        return std::unexpected(rich_error("encode failed"));
    }
    if(encoded->empty()) {
        return std::unexpected(rich_error("empty buffer"));
    }
    return from_flatbuffer<T>(*encoded);
}

TEST_SUITE(fbs_decode_visitor) {

TEST_CASE(scalar_int32_roundtrip) {
    std::int32_t input = 42;
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_CASE(scalar_string_roundtrip) {
    std::string input = "hello world";
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello world");
}

TEST_CASE(scalar_bool_roundtrip) {
    bool input = true;
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, true);
}

TEST_CASE(scalar_double_roundtrip) {
    double input = 3.14;
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 3.14);
}

TEST_CASE(simple_struct_roundtrip) {
    address input{.city = "tokyo", .zip = 100};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->city, "tokyo");
    EXPECT_EQ(result->zip, 100);
}

TEST_CASE(inline_struct_roundtrip) {
    struct with_point {
        point pos;
    };

    with_point input{
        .pos = {.x = 10, .y = 20}
    };
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pos.x, 10);
    EXPECT_EQ(result->pos.y, 20);
}

TEST_CASE(bare_inline_struct_roundtrip) {
    point input{.x = 42, .y = 99};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->x, 42);
    EXPECT_EQ(result->y, 99);
}

TEST_CASE(struct_with_nested_struct_and_vector_roundtrip) {
    struct simple_person {
        std::int32_t id;
        std::string name;
        std::vector<std::int32_t> scores;
        address addr;
    };

    simple_person input{
        .id = 7,
        .name = "alice",
        .scores = {1, 2, 3},
        .addr = {.city = "sh", .zip = 200000},
    };
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 7);
    EXPECT_EQ(result->name, "alice");
    ASSERT_EQ(result->scores.size(), 3U);
    EXPECT_EQ(result->scores[0], 1);
    EXPECT_EQ(result->scores[1], 2);
    EXPECT_EQ(result->scores[2], 3);
    EXPECT_EQ(result->addr.city, "sh");
    EXPECT_EQ(result->addr.zip, 200000);
}

TEST_CASE(struct_with_vector_roundtrip) {
    person input{
        .id = 7,
        .name = "alice",
        .pos = {.x = 10, .y = 20},
        .scores = {1, 2, 3},
        .addr = {.city = "sh", .zip = 200000},
    };
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 7);
    EXPECT_EQ(result->name, "alice");
    EXPECT_EQ(result->pos.x, 10);
    EXPECT_EQ(result->pos.y, 20);
    ASSERT_EQ(result->scores.size(), 3U);
    EXPECT_EQ(result->scores[0], 1);
    EXPECT_EQ(result->scores[1], 2);
    EXPECT_EQ(result->scores[2], 3);
    EXPECT_EQ(result->addr.city, "sh");
    EXPECT_EQ(result->addr.zip, 200000);
}

TEST_CASE(vector_of_strings_roundtrip) {
    std::vector<std::string> input{"hello", "world", "foo"};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3U);
    EXPECT_EQ((*result)[0], "hello");
    EXPECT_EQ((*result)[1], "world");
    EXPECT_EQ((*result)[2], "foo");
}

TEST_CASE(vector_of_ints_roundtrip) {
    std::vector<std::int32_t> input{10, 20, 30};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3U);
    EXPECT_EQ((*result)[0], 10);
    EXPECT_EQ((*result)[1], 20);
    EXPECT_EQ((*result)[2], 30);
}

TEST_CASE(optional_present_roundtrip) {
    struct with_opt {
        std::optional<std::int32_t> value;
    };

    with_opt input{.value = 42};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->value.has_value());
    EXPECT_EQ(*result->value, 42);
}

TEST_CASE(optional_absent_roundtrip) {
    struct with_opt {
        std::optional<std::int32_t> value;
    };

    with_opt input{.value = std::nullopt};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->value.has_value());
}

TEST_CASE(pair_roundtrip) {
    std::pair<std::int32_t, std::string> input{42, "hello"};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 42);
    EXPECT_EQ(result->second, "hello");
}

TEST_CASE(tuple_roundtrip) {
    std::tuple<std::int32_t, std::string, double> input{42, "hello", 3.14};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<0>(*result), 42);
    EXPECT_EQ(std::get<1>(*result), "hello");
    EXPECT_EQ(std::get<2>(*result), 3.14);
}

TEST_CASE(variant_int_roundtrip) {
    struct with_var {
        std::variant<std::int32_t, std::string> value;
    };

    with_var input{.value = 42};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::int32_t>(result->value));
    EXPECT_EQ(std::get<std::int32_t>(result->value), 42);
}

TEST_CASE(variant_string_roundtrip) {
    struct with_var {
        std::variant<std::int32_t, std::string> value;
    };

    with_var input{.value = std::string("hello")};
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(result->value));
    EXPECT_EQ(std::get<std::string>(result->value), "hello");
}

TEST_CASE(map_roundtrip) {
    std::map<std::string, std::int32_t> input{
        {"a", 1},
        {"b", 2},
        {"c", 3}
    };
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3U);
    EXPECT_EQ(result->at("a"), 1);
    EXPECT_EQ(result->at("b"), 2);
    EXPECT_EQ(result->at("c"), 3);
}

TEST_CASE(vector_of_structs_roundtrip) {
    std::vector<address> input{
        {.city = "tokyo", .zip = 100},
        {.city = "osaka", .zip = 200},
    };
    auto result = roundtrip(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2U);
    EXPECT_EQ((*result)[0].city, "tokyo");
    EXPECT_EQ((*result)[0].zip, 100);
    EXPECT_EQ((*result)[1].city, "osaka");
    EXPECT_EQ((*result)[1].zip, 200);
}

};  // TEST_SUITE(fbs_decode_visitor)

// ---------------------------------------------------------------------------
// Diagnostic tests: step-by-step encode → raw read → decode for failing cases
// ---------------------------------------------------------------------------

struct Basic {
    bool is_valid{};
    std::int32_t i32{};
    double f64{};
    std::string text;
    auto operator==(const Basic&) const -> bool = default;
};

using scalar_tuple_t = std::tuple<bool,
                                  char,
                                  std::int8_t,
                                  std::uint8_t,
                                  std::int16_t,
                                  std::uint16_t,
                                  std::int32_t,
                                  std::uint32_t,
                                  std::int64_t,
                                  std::uint64_t,
                                  float,
                                  double,
                                  std::string>;

TEST_SUITE(fbs_decode_diagnostic) {

TEST_CASE(diag_pair_scalar_tuple) {
    using pair_t = std::pair<scalar_tuple_t, scalar_tuple_t>;
    const scalar_tuple_t a{true,
                           'q',
                           std::int8_t(-12),
                           std::uint8_t(210),
                           std::int16_t(-1024),
                           std::uint16_t(4096),
                           std::int32_t(-123456),
                           std::uint32_t(123456),
                           std::int64_t(-9876543210LL),
                           std::uint64_t(9876543210ULL),
                           1.5F,
                           -9.25,
                           std::string("scalar-a")};
    const scalar_tuple_t b{false,
                           'z',
                           std::int8_t(-8),
                           std::uint8_t(8),
                           std::int16_t(-16),
                           std::uint16_t(16),
                           std::int32_t(-32),
                           std::uint32_t(32),
                           std::int64_t(-64),
                           std::uint64_t(64),
                           -0.75F,
                           -12.125,
                           std::string("scalar-b")};

    pair_t input{a, b};
    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    // Step 1: Read the root table
    const auto* data = encoded->data();
    const auto* root = ::flatbuffers::GetRoot<fbs::Table>(data);
    ASSERT_TRUE(root != nullptr);

    // Step 2: Check that root has 2 pointer fields (pair elements)
    bool has_field0 = root->GetOptionalFieldOffset(fbs::detail::first_field) != 0;
    bool has_field1 =
        root->GetOptionalFieldOffset(fbs::detail::first_field + fbs::detail::field_step) != 0;
    EXPECT_TRUE(has_field0);
    EXPECT_TRUE(has_field1);

    // Step 3: Follow pointer for first element
    const auto* child0 = root->GetPointer<const fbs::Table*>(fbs::detail::first_field);
    ASSERT_TRUE(child0 != nullptr);

    // Check first element's bool field (index 0)
    auto bool_val = child0->GetField<std::uint8_t>(fbs::detail::first_field, 0);
    EXPECT_EQ(bool_val, 1);  // true

    // Check first element's int32 field (index 6)
    auto i32_val =
        child0->GetField<std::int32_t>(fbs::detail::first_field + fbs::detail::field_step * 6, 0);
    EXPECT_EQ(i32_val, -123456);

    // Check first element's string field (index 12)
    const auto* str_ptr = child0->GetPointer<const fbs::String*>(fbs::detail::first_field +
                                                                 fbs::detail::field_step * 12);
    ASSERT_TRUE(str_ptr != nullptr);
    EXPECT_EQ(std::string_view(str_ptr->data(), str_ptr->size()), "scalar-a");

    // Step 4: Decode roundtrip
    pair_t output{};
    auto decode_result = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(decode_result.has_value());
    EXPECT_EQ(std::get<0>(output.first), true);          // bool
    EXPECT_EQ(std::get<1>(output.first), 'q');           // char
    EXPECT_EQ(std::get<6>(output.first), -123456);       // int32
    EXPECT_EQ(std::get<12>(output.first), "scalar-a");   // string
    EXPECT_EQ(std::get<0>(output.second), false);        // bool
    EXPECT_EQ(std::get<12>(output.second), "scalar-b");  // string
    EXPECT_EQ(input, output);
}

TEST_CASE(diag_variant_vector_int) {
    using var_t = std::variant<std::tuple<int, std::string>, std::vector<int>, Basic>;
    var_t input{
        std::vector<int>{1, 2, 3}
    };

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    // Step 1: Read root table (variant table)
    const auto* data = encoded->data();
    const auto* root = ::flatbuffers::GetRoot<fbs::Table>(data);
    ASSERT_TRUE(root != nullptr);

    // Step 2: Read variant tag at first_field
    auto tag = root->GetField<std::uint32_t>(fbs::detail::first_field, 0);
    EXPECT_EQ(tag, 1U);  // vector<int> is at index 1

    // Step 3: Compute payload slot
    auto payload_slot =
        static_cast<fbs::voffset_t>(fbs::detail::first_field + fbs::detail::field_step * (tag + 1));

    // Step 4: Check what's at the payload slot
    bool has_payload = root->GetOptionalFieldOffset(payload_slot) != 0;
    EXPECT_TRUE(has_payload);

    // Step 5: Try reading as vector<int32_t> directly
    const auto* vec = root->GetPointer<const fbs::Vector<std::int32_t>*>(payload_slot);
    if(vec != nullptr) {
        EXPECT_EQ(vec->size(), 3U);
        if(vec->size() >= 3) {
            EXPECT_EQ(vec->Get(0), 1);
            EXPECT_EQ(vec->Get(1), 2);
            EXPECT_EQ(vec->Get(2), 3);
        }
    } else {
        // Maybe it's wrapped in a table?
        const auto* wrapper = root->GetPointer<const fbs::Table*>(payload_slot);
        if(wrapper != nullptr) {
            const auto* inner_vec =
                wrapper->GetPointer<const fbs::Vector<std::int32_t>*>(fbs::detail::first_field);
            ASSERT_TRUE(inner_vec != nullptr);
            EXPECT_EQ(inner_vec->size(), 3U);
        }
    }

    // Step 6: Decode roundtrip
    var_t output{};
    auto decode_result = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(decode_result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<int>>(output));
    auto& decoded_vec = std::get<std::vector<int>>(output);
    ASSERT_EQ(decoded_vec.size(), 3U);
    EXPECT_EQ(decoded_vec[0], 1);
    EXPECT_EQ(decoded_vec[1], 2);
    EXPECT_EQ(decoded_vec[2], 3);
}

TEST_CASE(diag_variant_monostate) {
    using var_t = std::variant<std::monostate, int, double, std::string, Basic>;
    var_t input{std::in_place_index<0>};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    const auto* data = encoded->data();
    const auto* root = ::flatbuffers::GetRoot<fbs::Table>(data);
    ASSERT_TRUE(root != nullptr);

    auto tag = root->GetField<std::uint32_t>(fbs::detail::first_field, 0);
    EXPECT_EQ(tag, 0U);

    var_t output{42};  // start with different value
    auto decode_result = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(decode_result.has_value());
    EXPECT_EQ(output.index(), 0U);
}

TEST_CASE(diag_variant_basic) {
    using var_t = std::variant<std::monostate, int, double, std::string, Basic>;
    var_t input{
        Basic{.is_valid = true, .i32 = 64, .f64 = 2.5, .text = "variant-basic"}
    };

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    const auto* data = encoded->data();
    const auto* root = ::flatbuffers::GetRoot<fbs::Table>(data);
    ASSERT_TRUE(root != nullptr);

    auto tag = root->GetField<std::uint32_t>(fbs::detail::first_field, 0);
    EXPECT_EQ(tag, 4U);  // Basic is at index 4

    var_t output{};
    auto decode_result = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(decode_result.has_value());
    ASSERT_TRUE(std::holds_alternative<Basic>(output));
    EXPECT_EQ(std::get<Basic>(output).is_valid, true);
    EXPECT_EQ(std::get<Basic>(output).i32, 64);
    EXPECT_EQ(std::get<Basic>(output).text, "variant-basic");
}

TEST_CASE(diag_tagged_ext_variant_int) {
    using ext_t =
        meta::annotation<std::variant<int, std::string, Basic>,
                         meta::attrs::externally_tagged::names<"integer", "text", "basic">>;
    ext_t input{42};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    const auto* data = encoded->data();
    const auto* root = ::flatbuffers::GetRoot<fbs::Table>(data);
    ASSERT_TRUE(root != nullptr);

    // For non-human-readable, tagged variants use the same wire format as untagged
    auto tag = root->GetField<std::uint32_t>(fbs::detail::first_field, 0);
    EXPECT_EQ(tag, 0U);  // int is at index 0

    auto payload_slot =
        static_cast<fbs::voffset_t>(fbs::detail::first_field + fbs::detail::field_step * 1);
    auto int_val = root->GetField<std::int32_t>(payload_slot, 0);
    EXPECT_EQ(int_val, 42);

    ext_t output{};
    auto decode_result = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(decode_result.has_value());
    ASSERT_TRUE(std::holds_alternative<int>(meta::annotated_value(output)));
    EXPECT_EQ(std::get<int>(meta::annotated_value(output)), 42);
}

TEST_CASE(diag_tuple_01_pair_int_string) {
    auto r = roundtrip(std::pair<int, std::string>{9, "pair"});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->first, 9);
    EXPECT_EQ(r->second, "pair");
}

TEST_CASE(diag_tuple_02_pair_u64_bool) {
    auto r = roundtrip(std::pair<std::uint64_t, bool>{42ULL, true});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->first, 42ULL);
    EXPECT_EQ(r->second, true);
}

TEST_CASE(diag_tuple_03_pair_basic_basic) {
    auto r = roundtrip(std::pair<Basic, Basic>{
        Basic{true,  11,  1.5,  "lhs"},
        Basic{false, -22, -2.5, "rhs"}
    });
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->first.i32, 11);
    EXPECT_EQ(r->second.i32, -22);
}

TEST_CASE(diag_tuple_04_tuple_int_bool_string) {
    auto r = roundtrip(std::tuple<int, bool, std::string>{7, true, "tuple"});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(std::get<0>(*r), 7);
    EXPECT_EQ(std::get<1>(*r), true);
    EXPECT_EQ(std::get<2>(*r), "tuple");
}

TEST_CASE(diag_tuple_05_scalar_tuple) {
    scalar_tuple_t input{true,
                         'q',
                         std::int8_t(-12),
                         std::uint8_t(210),
                         std::int16_t(-1024),
                         std::uint16_t(4096),
                         std::int32_t(-123456),
                         std::uint32_t(123456),
                         std::int64_t(-9876543210LL),
                         std::uint64_t(9876543210ULL),
                         1.5F,
                         -9.25,
                         std::string("all-scalars-a")};
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(input, *r);
}

TEST_CASE(diag_tuple_06_tuple_basic_array_pair) {
    using T = std::tuple<Basic, std::array<int, 3>, std::pair<std::uint32_t, float>>;
    T input{
        Basic{true, 33, 3.75, "tuple-struct"},
        std::array<int, 3>{1, 3, 5},
        std::pair<std::uint32_t, float>{99U, 6.25F}
    };

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    const auto* root = ::flatbuffers::GetRoot<fbs::Table>(encoded->data());
    ASSERT_TRUE(root != nullptr);

    // Root is a tuple table with 3 pointer fields
    // Field 0 (Basic): table pointer at slot 4
    // Field 1 (array<int,3>): table pointer at slot 6
    // Field 2 (pair<uint32_t,float>): table pointer at slot 8
    const auto* child1 =
        root->GetPointer<const fbs::Table*>(fbs::detail::first_field + fbs::detail::field_step * 1);
    ASSERT_TRUE(child1 != nullptr);

    // child1 should be a sub-table with 3 int fields at slots 4, 6, 8
    auto v0 = child1->GetField<std::int32_t>(fbs::detail::first_field, 0);
    auto v1 = child1->GetField<std::int32_t>(fbs::detail::first_field + fbs::detail::field_step, 0);
    auto v2 =
        child1->GetField<std::int32_t>(fbs::detail::first_field + fbs::detail::field_step * 2, 0);
    EXPECT_EQ(v0, 1);
    EXPECT_EQ(v1, 3);
    EXPECT_EQ(v2, 5);

    // Manual decode step-by-step
    T output{};
    auto decode_result = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(decode_result.has_value());

    // Check each tuple element
    EXPECT_EQ(std::get<0>(output).is_valid, true);
    EXPECT_EQ(std::get<0>(output).i32, 33);
    EXPECT_EQ(std::get<0>(output).text, "tuple-struct");
    EXPECT_EQ(std::get<1>(output)[0], 1);
    EXPECT_EQ(std::get<1>(output)[1], 3);
    EXPECT_EQ(std::get<1>(output)[2], 5);
    EXPECT_EQ(std::get<2>(output).first, 99U);
    EXPECT_EQ(std::get<2>(output).second, 6.25F);

    // Manual decode of element 1 via field_reader directly
    fbs::decode_detail::FieldReader vr{
        root,
        static_cast<fbs::voffset_t>(fbs::detail::first_field + fbs::detail::field_step)};
    // This should follow the pointer at slot 6 to get the sub-table
    std::array<int, 3> manual_arr{};
    bool manual_ok = vr.visit_tuple(manual_arr, [&](auto& sv) -> bool {
        for(std::size_t i = 0; i < 3; ++i) {
            bool ok =
                sv.visit_element([&](auto& ev) -> bool { return ev.visit_int(manual_arr[i]); });
            if(!ok)
                return false;
        }
        return true;
    });
    ASSERT_TRUE(manual_ok);
    EXPECT_EQ(manual_arr[0], 1);
    EXPECT_EQ(manual_arr[1], 3);
    EXPECT_EQ(manual_arr[2], 5);
}

TEST_CASE(diag_tuple_07_array_int3) {
    auto r = roundtrip(std::array<int, 3>{4, 5, 6});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ((*r)[0], 4);
    EXPECT_EQ((*r)[1], 5);
    EXPECT_EQ((*r)[2], 6);
}

TEST_CASE(diag_tuple_08_array_basic2) {
    auto r = roundtrip(std::array<Basic, 2>{
        Basic{true,  101,  10.1,  "arr-a"},
        Basic{false, -202, -20.2, "arr-b"}
    });
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ((*r)[0].i32, 101);
    EXPECT_EQ((*r)[1].i32, -202);
}

TEST_CASE(diag_tuple_09_pair_scalar_tuple_scalar_tuple) {
    // Already tested - just confirm it still passes
    using pair_t = std::pair<scalar_tuple_t, scalar_tuple_t>;
    const scalar_tuple_t a{true,
                           'q',
                           std::int8_t(-12),
                           std::uint8_t(210),
                           std::int16_t(-1024),
                           std::uint16_t(4096),
                           std::int32_t(-123456),
                           std::uint32_t(123456),
                           std::int64_t(-9876543210LL),
                           std::uint64_t(9876543210ULL),
                           1.5F,
                           -9.25,
                           std::string("all-scalars-a")};
    const scalar_tuple_t b{false,
                           'z',
                           std::int8_t(-8),
                           std::uint8_t(8),
                           std::int16_t(-16),
                           std::uint16_t(16),
                           std::int32_t(-32),
                           std::uint32_t(32),
                           std::int64_t(-64),
                           std::uint64_t(64),
                           -0.75F,
                           -12.125,
                           std::string("all-scalars-b")};
    auto r = roundtrip(pair_t{a, b});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->first, a);
    EXPECT_EQ(r->second, b);
}

TEST_CASE(diag_tuple_10_array_scalar_tuple2) {
    const scalar_tuple_t a{true,
                           'q',
                           std::int8_t(-12),
                           std::uint8_t(210),
                           std::int16_t(-1024),
                           std::uint16_t(4096),
                           std::int32_t(-123456),
                           std::uint32_t(123456),
                           std::int64_t(-9876543210LL),
                           std::uint64_t(9876543210ULL),
                           1.5F,
                           -9.25,
                           std::string("all-scalars-a")};
    const scalar_tuple_t b{false,
                           'z',
                           std::int8_t(-8),
                           std::uint8_t(8),
                           std::int16_t(-16),
                           std::uint16_t(16),
                           std::int32_t(-32),
                           std::uint32_t(32),
                           std::int64_t(-64),
                           std::uint64_t(64),
                           -0.75F,
                           -12.125,
                           std::string("all-scalars-b")};
    auto r = roundtrip(std::array<scalar_tuple_t, 2>{a, b});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ((*r)[0], a);
    EXPECT_EQ((*r)[1], b);
}

TEST_CASE(diag_struct_with_array_field) {
    struct WithArray {
        std::int32_t x;
        std::array<float, 3> arr;
        std::string s;
    };

    WithArray input{
        .x = 42,
        .arr = {1.5F, -2.25F, 0.0F},
        .s = "test"
    };
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->x, 42);
    EXPECT_EQ(r->arr[0], 1.5F);
    EXPECT_EQ(r->arr[1], -2.25F);
    EXPECT_EQ(r->arr[2], 0.0F);
    EXPECT_EQ(r->s, "test");
}

TEST_CASE(diag_complex_01_scalars) {
    auto r = roundtrip(standard_case::make_scalars());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->b, true);
    EXPECT_EQ(r->s, "hello scalars");
}

TEST_CASE(diag_complex_02_nested_containers) {
    auto input = standard_case::make_nested_containers();
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(input, *r);
}

TEST_CASE(diag_complex_03_empty_containers) {
    auto input = standard_case::make_empty_containers();
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(input, *r);
}

TEST_CASE(diag_complex_04_ultimate) {
    auto input = standard_case::make_ultimate();
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(input.basic, r->basic);
    EXPECT_EQ(input.compound.string_list, r->compound.string_list);
    EXPECT_EQ(input.compound.fixed_array, r->compound.fixed_array);
    EXPECT_EQ(input.compound.heterogeneous_tuple, r->compound.heterogeneous_tuple);
    EXPECT_EQ(input.compound, r->compound);
    EXPECT_EQ(input.hard_map, r->hard_map);
    EXPECT_EQ(input, *r);
}

TEST_CASE(diag_complex_05_ultimate_monostate) {
    auto input = standard_case::make_ultimate();
    input.adts.multi_variant = std::monostate{};
    input.nullables.opt_value.reset();
    input.nullables.heap_allocated.reset();
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(input, *r);
}

TEST_CASE(diag_complex_06_ultimate_int_variant) {
    auto input = standard_case::make_ultimate();
    input.adts.multi_variant = 123;
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(input, *r);
}

TEST_CASE(diag_complex_07_ultimate_string_variant) {
    auto input = standard_case::make_ultimate();
    input.adts.multi_variant = std::string("variant-text");
    auto r = roundtrip(input);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(input, *r);
}

};  // TEST_SUITE(fbs_decode_diagnostic)

}  // namespace

}  // namespace kota::codec

#endif
