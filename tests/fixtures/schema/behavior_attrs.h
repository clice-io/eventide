#pragma once

// Behavior-attr fixtures — skip_if / with / as / enum_string and combinations.

#include <optional>
#include <string>
#include <vector>

#include "fixtures/schema/enums.h"
#include "fixtures/schema/primitives.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/codec/macro.h"

namespace kota::meta::fixtures {

struct IntToStringAdapter {
    using type = std::string;
};

struct BytesAdapter {
    using type = std::vector<std::byte>;
};

struct BehaviorStruct {
    annotation<std::optional<int>, behavior::skip_if<pred::optional_none>> maybe;
    annotation<int, behavior::as<std::string>> as_str;
    float plain;
};

struct WithReprStruct {
    annotation<int, behavior::with<IntToStringAdapter>> converted;
    float plain;
};

struct WithCompoundReprStruct {
    annotation<int, behavior::with<BytesAdapter>> chunk;
};

struct AsVectorStruct {
    annotation<int, behavior::as<std::vector<int>>> value;
};

struct AsStructStruct {
    annotation<int, behavior::as<SimpleStruct>> value;
};

struct AsOptionalStruct {
    annotation<int, behavior::as<std::optional<int>>> value;
};

struct EnumStringStruct {
    annotation<Color, behavior::enum_string<rename_policy::identity>> color_field;
    int count;
};

struct EnumStringCamelStruct {
    annotation<Color, behavior::enum_string<rename_policy::lower_camel>> color_field;
};

struct EnumStringUpperSnakeStruct {
    annotation<Color, behavior::enum_string<rename_policy::upper_snake>> color_field;
};

struct SkipIfEmptyStringStruct {
    annotation<std::string, behavior::skip_if<pred::empty>> s;
};

struct SkipIfEmptyVectorStruct {
    annotation<std::vector<int>, behavior::skip_if<pred::empty>> xs;
};

struct SkipIfDefaultIntStruct {
    annotation<int, behavior::skip_if<pred::default_value>> x;
};

struct IsNegative {
    constexpr bool operator()(const int& v) const {
        return v < 0;
    }
};

struct SkipIfCustomStruct {
    annotation<int, behavior::skip_if<IsNegative>> maybe_negative;
};

struct MultiAttrStruct {
    KOTATSU_ANNOTATE(defaulted = true, skip_if = type<pred::optional_none>)
    <std::optional<int>> opt_with_default;
    KOTATSU_ANNOTATE(rename = "score", as = type<std::string>)
    <int> renamed_as;
};

struct SkipIfAsStruct {
    annotation<std::optional<std::string>,
               behavior::skip_if<pred::optional_none>,
               behavior::as<std::string>>
        field;
};

struct SkipIfWithStruct {
    annotation<std::optional<int>,
               behavior::skip_if<pred::optional_none>,
               behavior::with<IntToStringAdapter>>
        field;
};

}  // namespace kota::meta::fixtures
