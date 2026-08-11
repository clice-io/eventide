#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "fixtures/schema/behavior_attrs.h"
#include "fixtures/schema/tagged.h"
#include "kota/zest/zest.h"
#include "kota/meta/attrs.h"
#include "kota/meta/schema.h"

namespace kota::meta {

using kota::type_list_element_t;

namespace {

namespace fx = ::kota::meta::fixtures;

TEST_SUITE(virtual_schema_behavior_attrs) {

TEST_CASE(skip_if_and_as) {
    STATIC_EXPECT_EQ(virtual_schema<fx::BehaviorStruct>::count, 3U);

    constexpr auto& fields = virtual_schema<fx::BehaviorStruct>::fields;

    STATIC_EXPECT_EQ(fields[0].name, "maybe");
    STATIC_EXPECT_EQ(fields[1].name, "as_str");
    STATIC_EXPECT_EQ(fields[2].name, "plain");

    // skip_if is not a behavior provider
    STATIC_EXPECT_TRUE(fields[0].has_skip_if);
    STATIC_EXPECT_FALSE(fields[0].has_behavior);

    // as<string> is a behavior provider; encoded type becomes string
    STATIC_EXPECT_TRUE(fields[1].has_behavior);
    STATIC_EXPECT_FALSE(fields[1].has_skip_if);
    STATIC_EXPECT_EQ(fields[1].type().kind, type_kind::string);

    // plain field: no behavior flags
    STATIC_EXPECT_FALSE(fields[2].has_skip_if);
    STATIC_EXPECT_FALSE(fields[2].has_behavior);
}

TEST_CASE(with_adapter_type_info) {
    constexpr auto& fields = virtual_schema<fx::WithReprStruct>::fields;

    // Adapter declares type = std::string
    STATIC_EXPECT_EQ(fields[0].type().kind, type_kind::string);
    STATIC_EXPECT_TRUE(fields[0].has_behavior);

    // The slot keeps the raw field type; repr resolution is pinned by the
    // field's type_info kind above.
    using slots = virtual_schema<fx::WithReprStruct>::slots;
    using slot0 = type_list_element_t<0, slots>;
    EXPECT_TYPE_EQ(slot0::raw_type, int);

    // plain float is unaffected
    STATIC_EXPECT_EQ(fields[1].type().kind, type_kind::float32);
    STATIC_EXPECT_FALSE(fields[1].has_behavior);
}

TEST_CASE(enum_string) {
    constexpr auto& fields = virtual_schema<fx::EnumStringStruct>::fields;

    // enum_string encodes through a non-owning view
    STATIC_EXPECT_EQ(fields[0].type().kind, type_kind::string);
    STATIC_EXPECT_TRUE(fields[0].has_behavior);
    EXPECT_TYPE_EQ(resolved_repr_t<decltype(fx::EnumStringStruct::color_field)>, std::string_view);

    // Verify slot types
    using slots = virtual_schema<fx::EnumStringStruct>::slots;
    using slot0 = type_list_element_t<0, slots>;
    EXPECT_TYPE_EQ(slot0::raw_type, fx::Color);

    // plain int is unaffected
    STATIC_EXPECT_EQ(fields[1].type().kind, type_kind::int32);
    STATIC_EXPECT_FALSE(fields[1].has_behavior);
}

TEST_CASE(tagged_variant) {
    constexpr auto& fields = virtual_schema<fx::TaggedVariantStruct>::fields;
    STATIC_EXPECT_EQ(fields[0].type().kind, type_kind::variant);

    // The slot keeps the raw variant type; the tagged<> spec itself is
    // asserted in slots_tests (tagged_variant_slot).
    using slots = virtual_schema<fx::TaggedVariantStruct>::slots;
    using slot0 = type_list_element_t<0, slots>;
    EXPECT_TYPE_EQ(slot0::raw_type, std::variant<int, std::string>);
}

TEST_CASE(multi_attr_combination) {
    constexpr auto& fields = virtual_schema<fx::MultiAttrStruct>::fields;

    // opt_with_default: has_default=true, has_skip_if=true, has_behavior=false
    STATIC_EXPECT_TRUE(fields[0].has_default);
    STATIC_EXPECT_TRUE(fields[0].has_skip_if);
    STATIC_EXPECT_FALSE(fields[0].has_behavior);

    // renamed_as: name="score", has_behavior=true, type->kind=string
    STATIC_EXPECT_EQ(fields[1].name, "score");
    STATIC_EXPECT_TRUE(fields[1].has_behavior);
    STATIC_EXPECT_EQ(fields[1].type().kind, type_kind::string);
}

TEST_CASE(skip_if_combined_with_behavior) {
    // skip_if + as: both flags present
    {
        constexpr auto& fields = virtual_schema<fx::SkipIfAsStruct>::fields;
        STATIC_EXPECT_TRUE(fields[0].has_skip_if);
        STATIC_EXPECT_TRUE(fields[0].has_behavior);
        STATIC_EXPECT_EQ(fields[0].type().kind, type_kind::string);
    }

    // skip_if + with: both flags present, encoded type = string (from adapter)
    {
        constexpr auto& fields = virtual_schema<fx::SkipIfWithStruct>::fields;
        STATIC_EXPECT_TRUE(fields[0].has_skip_if);
        STATIC_EXPECT_TRUE(fields[0].has_behavior);
        STATIC_EXPECT_EQ(fields[0].type().kind, type_kind::string);
    }
}

};  // TEST_SUITE(virtual_schema_behavior_attrs)

}  // namespace

}  // namespace kota::meta
