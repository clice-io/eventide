#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/serde/schema/virtual_schema.h"

namespace eventide::serde {

using schema::type_kind;
using schema::type_info;
using schema::type_info_instance;
using schema::array_type_info;
using schema::map_type_info;
using schema::optional_type_info;
using schema::variant_type_info;
using schema::tuple_type_info;
using config::default_config;

namespace test_schema {

enum class color { red, green, blue };

struct SimpleStruct {
    int x;
    std::string name;
    float score;
};

}  // namespace test_schema

namespace {

TEST_SUITE(virtual_schema_type_info) {

TEST_CASE(scalar_helpers) {
    // int32: signed integer, numeric, scalar
    constexpr auto& int_info = type_info_instance<int, default_config>::value;
    EXPECT_EQ(int_info.kind, type_kind::int32);
    EXPECT_TRUE(int_info.is_integer());
    EXPECT_TRUE(int_info.is_signed_integer());
    EXPECT_FALSE(int_info.is_unsigned_integer());
    EXPECT_FALSE(int_info.is_floating());
    EXPECT_TRUE(int_info.is_numeric());
    EXPECT_TRUE(int_info.is_scalar());

    // uint64: unsigned integer, numeric, scalar
    constexpr auto& u64_info = type_info_instance<std::uint64_t, default_config>::value;
    EXPECT_TRUE(u64_info.is_integer());
    EXPECT_FALSE(u64_info.is_signed_integer());
    EXPECT_TRUE(u64_info.is_unsigned_integer());
    EXPECT_TRUE(u64_info.is_numeric());
    EXPECT_TRUE(u64_info.is_scalar());

    // double: floating, numeric, scalar
    constexpr auto& dbl_info = type_info_instance<double, default_config>::value;
    EXPECT_FALSE(dbl_info.is_integer());
    EXPECT_TRUE(dbl_info.is_floating());
    EXPECT_TRUE(dbl_info.is_numeric());
    EXPECT_TRUE(dbl_info.is_scalar());

    // bool: scalar but not numeric
    constexpr auto& bool_info = type_info_instance<bool, default_config>::value;
    EXPECT_TRUE(bool_info.is_scalar());
    EXPECT_FALSE(bool_info.is_numeric());
    EXPECT_FALSE(bool_info.is_integer());
    EXPECT_FALSE(bool_info.is_floating());

    // string: scalar but not numeric
    constexpr auto& str_info = type_info_instance<std::string, default_config>::value;
    EXPECT_EQ(str_info.kind, type_kind::string);
    EXPECT_TRUE(str_info.is_scalar());
    EXPECT_FALSE(str_info.is_numeric());

    // enum: scalar but not numeric
    constexpr auto& enum_info = type_info_instance<test_schema::color, default_config>::value;
    EXPECT_EQ(enum_info.kind, type_kind::enumeration);
    EXPECT_TRUE(enum_info.is_scalar());
    EXPECT_FALSE(enum_info.is_numeric());
}

TEST_CASE(compound_types) {
    // vector<int> -> array with int32 element
    {
        constexpr auto& info = type_info_instance<std::vector<int>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::array);
        EXPECT_FALSE(info.is_scalar());
        auto* arr = static_cast<const array_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(arr->element->kind, type_kind::int32);
    }

    // set<int> -> set with int32 element
    {
        constexpr auto& info = type_info_instance<std::set<int>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::set);
        EXPECT_FALSE(info.is_scalar());
        auto* arr = static_cast<const array_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(arr->element->kind, type_kind::int32);
    }

    // map<string, int> -> map with string key, int32 value
    {
        constexpr auto& info = type_info_instance<std::map<std::string, int>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::map);
        EXPECT_FALSE(info.is_scalar());
        auto* m = static_cast<const map_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(m->key->kind, type_kind::string);
        EXPECT_EQ(m->value->kind, type_kind::int32);
    }

    // optional<int> -> optional with int32 inner
    {
        constexpr auto& info = type_info_instance<std::optional<int>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::optional);
        auto* opt = static_cast<const optional_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(opt->inner->kind, type_kind::int32);
    }

    // unique_ptr<int> -> pointer with int32 inner
    {
        constexpr auto& info = type_info_instance<std::unique_ptr<int>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::pointer);
        auto* ptr = static_cast<const optional_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(ptr->inner->kind, type_kind::int32);
    }

    // shared_ptr<string> -> pointer with string inner
    {
        constexpr auto& info = type_info_instance<std::shared_ptr<std::string>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::pointer);
        auto* ptr = static_cast<const optional_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(ptr->inner->kind, type_kind::string);
    }

    // variant<int, string> -> variant with 2 alternatives
    {
        constexpr auto& info = type_info_instance<std::variant<int, std::string>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::variant);
        auto* var = static_cast<const variant_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(var->alternatives.size(), 2U);
        EXPECT_EQ(var->alternatives[0]->kind, type_kind::int32);
        EXPECT_EQ(var->alternatives[1]->kind, type_kind::string);
    }

    // pair<int, float> -> tuple with 2 elements
    {
        constexpr auto& info = type_info_instance<std::pair<int, float>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::tuple);
        auto* tup = static_cast<const tuple_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(tup->elements.size(), 2U);
        EXPECT_EQ(tup->elements[0]->kind, type_kind::int32);
        EXPECT_EQ(tup->elements[1]->kind, type_kind::float32);
    }

    // tuple<int, double, string> -> tuple with 3 elements
    {
        constexpr auto& info = type_info_instance<std::tuple<int, double, std::string>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::tuple);
        auto* tup = static_cast<const tuple_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(tup->elements.size(), 3U);
        EXPECT_EQ(tup->elements[0]->kind, type_kind::int32);
        EXPECT_EQ(tup->elements[1]->kind, type_kind::float64);
        EXPECT_EQ(tup->elements[2]->kind, type_kind::string);
    }

    // SimpleStruct -> structure
    {
        constexpr auto& info = type_info_instance<test_schema::SimpleStruct, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::structure);
        EXPECT_FALSE(info.is_scalar());
    }
}

TEST_CASE(nested_type_info) {
    // vector<optional<int>> -> array -> optional -> int32
    {
        constexpr auto& info = type_info_instance<std::vector<std::optional<int>>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::array);
        auto* arr = static_cast<const array_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(arr->element->kind, type_kind::optional);
        auto* opt = static_cast<const optional_type_info*>(arr->element);
        EXPECT_EQ(opt->inner->kind, type_kind::int32);
    }

    // map<string, vector<int>> -> map -> string key, array value -> int32 element
    {
        constexpr auto& info = type_info_instance<std::map<std::string, std::vector<int>>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::map);
        auto* m = static_cast<const map_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(m->key->kind, type_kind::string);
        EXPECT_EQ(m->value->kind, type_kind::array);
        auto* inner_arr = static_cast<const array_type_info*>(m->value);
        EXPECT_EQ(inner_arr->element->kind, type_kind::int32);
    }

    // set<vector<string>> -> set -> array -> string
    {
        constexpr auto& info = type_info_instance<std::set<std::vector<std::string>>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::set);
        auto* s = static_cast<const array_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(s->element->kind, type_kind::array);
        auto* inner_arr = static_cast<const array_type_info*>(s->element);
        EXPECT_EQ(inner_arr->element->kind, type_kind::string);
    }

    // unique_ptr<vector<int>> -> pointer -> array -> int32
    {
        constexpr auto& info = type_info_instance<std::unique_ptr<std::vector<int>>, default_config>::value;
        EXPECT_EQ(info.kind, type_kind::pointer);
        auto* ptr = static_cast<const optional_type_info*>(static_cast<const type_info*>(&info));
        EXPECT_EQ(ptr->inner->kind, type_kind::array);
        auto* arr = static_cast<const array_type_info*>(ptr->inner);
        EXPECT_EQ(arr->element->kind, type_kind::int32);
    }
}

};  // TEST_SUITE(virtual_schema_type_info)

}  // namespace

}  // namespace eventide::serde
