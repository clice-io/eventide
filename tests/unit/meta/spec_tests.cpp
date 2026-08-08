#include <tuple>
#include <type_traits>

#include "kota/zest/zest.h"
#include "kota/meta/schema.h"

namespace kota::meta {

namespace {

struct full_tag {
    constexpr static auto spec = make_spec(dsl::rename = "userId",
                                           dsl::alias = {"user_id", "uid"},
                                           dsl::description = "User identifier.",
                                           dsl::idx = 7u);
};

struct defaulted_tag {
    constexpr static auto spec = make_spec(dsl::defaulted = true);
};

struct skip_tag {
    constexpr static auto spec = make_spec(dsl::skip = true);
};

struct flatten_tag {
    constexpr static auto spec = make_spec(dsl::flatten = true);
};

struct skip_none_tag {
    constexpr static auto spec = make_spec(dsl::skip_if = skip_when::none);
};

struct custom_pred {
    constexpr bool operator()(const int& value, bool is_serialize) const {
        return is_serialize && value < 0;
    }
};

struct extras_tag {
    constexpr static auto spec =
        make_spec(dsl::description = "Converted field.", dsl::as = dsl::type<std::int64_t>);
};

struct inner_pair {
    int first = 0;
    int second = 0;
};

struct spec_struct {
    annotate<full_tag>::type<int> user_id;
    annotate<skip_tag>::type<int> internal = 0;
    annotate<flatten_tag>::type<inner_pair> pair;
    annotate<defaulted_tag>::type<int> retries = 0;
};

TEST_SUITE(meta_spec) {

TEST_CASE(make_spec_folds_values) {
    constexpr const field_spec& spec = full_tag::spec.value;
    STATIC_EXPECT_EQ(spec.rename, "userId");
    STATIC_EXPECT_EQ(spec.description, "User identifier.");
    STATIC_EXPECT_EQ(spec.idx, 7u);
    STATIC_EXPECT_EQ(spec.alias.count, 2u);
    STATIC_EXPECT_EQ(spec.alias.storage[0], "user_id");
    STATIC_EXPECT_EQ(spec.alias.storage[1], "uid");
    STATIC_EXPECT_FALSE(spec.skip);
    STATIC_EXPECT_FALSE(spec.flatten);
    STATIC_EXPECT_FALSE(spec.defaulted);
    STATIC_EXPECT_TRUE(spec.skip_if == skip_when::never);
    STATIC_EXPECT_TRUE(std::tuple_size_v<decltype(full_tag::spec)::extras> == 0);
}

TEST_CASE(make_spec_keeps_type_components_in_type) {
    STATIC_EXPECT_EQ(extras_tag::spec.value.description, "Converted field.");
    using extras = decltype(extras_tag::spec)::extras;
    STATIC_EXPECT_TRUE(std::tuple_size_v<extras> == 1);
    STATIC_EXPECT_TRUE((std::is_same_v<std::tuple_element_t<0, extras>,
                                       dsl::type_component<dsl::aspect::as, std::int64_t>>));
}

TEST_CASE(annotate_maps_components_to_behavior_attrs) {
    using annotated = annotate<extras_tag>::type<std::int32_t>;
    STATIC_EXPECT_TRUE((std::is_same_v<typename annotated::annotated_type, std::int32_t>));
    using attrs_t = typename annotated::attrs;
    STATIC_EXPECT_TRUE((tuple_has_v<attrs_t, attrs::spec<extras_tag>>));
    STATIC_EXPECT_TRUE((tuple_has_v<attrs_t, behavior::as<std::int64_t>>));

    using with_extra =
        annotate<skip_none_tag>::type<std::optional<int>, behavior::skip_if<custom_pred>>;
    STATIC_EXPECT_TRUE((tuple_has_v<typename with_extra::attrs, behavior::skip_if<custom_pred>>));
}

TEST_CASE(spec_of_reads_annotation_and_defaults_to_empty) {
    using annotated = annotate<full_tag>::type<int>;
    STATIC_EXPECT_EQ(field_spec_of<annotated>.rename, "userId");
    STATIC_EXPECT_EQ(field_spec_of<int>.rename, "");
    STATIC_EXPECT_TRUE(&field_spec_of<int> == &empty_field_spec);
}

TEST_CASE(field_info_carries_spec_values) {
    using schema = virtual_schema<spec_struct>;
    // internal is skipped; pair flattens to two fields.
    STATIC_EXPECT_EQ(schema::count, 4u);

    constexpr auto& fields = schema::fields;
    STATIC_EXPECT_EQ(fields[0].name, "userId");
    STATIC_EXPECT_EQ(fields[0].aliases.size(), 2u);
    STATIC_EXPECT_EQ(fields[0].aliases[1], "uid");
    STATIC_EXPECT_EQ(fields[0].idx, 7u);
    STATIC_EXPECT_EQ(fields[0].description, "User identifier.");
    STATIC_EXPECT_FALSE(fields[0].has_default);

    STATIC_EXPECT_EQ(fields[1].name, "first");
    STATIC_EXPECT_EQ(fields[2].name, "second");

    STATIC_EXPECT_EQ(fields[3].name, "retries");
    STATIC_EXPECT_TRUE(fields[3].has_default);
    STATIC_EXPECT_EQ(fields[3].idx, field_spec::no_idx);
    STATIC_EXPECT_EQ(fields[3].description, "");
}

TEST_CASE(name_only_spec_shares_slot_with_bare_field) {
    // A rename/description-only spec is dropped from encode slots, so the
    // annotated field and a bare int share the slot type.
    using slots = virtual_schema<spec_struct>::slots;
    using first_slot = type_list_element_t<0, slots>;
    STATIC_EXPECT_TRUE((std::is_same_v<typename first_slot::attrs, std::tuple<>>));

    // A defaulted spec stays in the slot attrs for decode.
    using retries_slot = type_list_element_t<3, slots>;
    STATIC_EXPECT_TRUE((tuple_has_v<typename retries_slot::attrs, attrs::spec<defaulted_tag>>));
}

TEST_CASE(skip_when_evaluates_builtin_predicates) {
    STATIC_EXPECT_TRUE(evaluate_skip_when<skip_when::none>(std::optional<int>{}, true));
    STATIC_EXPECT_FALSE(evaluate_skip_when<skip_when::none>(std::optional<int>{1}, true));
    STATIC_EXPECT_TRUE(evaluate_skip_when<skip_when::empty>(std::string_view{}, true));
    STATIC_EXPECT_FALSE(evaluate_skip_when<skip_when::empty>(std::string_view{"x"}, true));
    STATIC_EXPECT_TRUE(evaluate_skip_when<skip_when::default_value>(0, true));
    STATIC_EXPECT_FALSE(evaluate_skip_when<skip_when::default_value>(1, true));
    // Deserialization never skips.
    STATIC_EXPECT_FALSE(evaluate_skip_when<skip_when::default_value>(0, false));
}

};  // TEST_SUITE(meta_spec)

}  // namespace

}  // namespace kota::meta
