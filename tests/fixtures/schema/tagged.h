#pragma once

#include <string>
#include <variant>

#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"

namespace kota::meta::fixtures {

struct Circle {
    double radius;
};

struct Rect {
    double width;
    double height;
};

struct TaggedIntCircle {
    int radius;
};

struct TaggedIntRect {
    int width;
    int height;
};

// Named annotation tags in the fixtures namespace so every including TU sees
// one type (required for shared headers). Hand-written make_struct_spec rather
// than the KOTATSU_ANNOTATION macro since this fixture header avoids macros.
struct ExternalTag {
    constexpr static auto spec =
        make_struct_spec(dsl::tagged = true, dsl::tag_names = {"integer", "text"});
};

struct InternalKindTag {
    constexpr static auto spec =
        make_struct_spec(dsl::tag = "kind", dsl::tag_names = {"circle", "rect"});
};

struct AdjacentTag {
    constexpr static auto spec = make_struct_spec(dsl::tag = "type",
                                                  dsl::content = "value",
                                                  dsl::tag_names = {"integer", "text"});
};

struct TaggedTag {
    constexpr static auto spec = make_struct_spec(dsl::tagged = true);
};

using ExternalTagged = annotate<ExternalTag>::type<std::variant<int, std::string>>;

using InternalTagged =
    annotate<InternalKindTag>::type<std::variant<TaggedIntCircle, TaggedIntRect>>;

using AdjacentTagged = annotate<AdjacentTag>::type<std::variant<int, std::string>>;

using TaggedRoot = annotate<InternalKindTag>::type<std::variant<Circle, Rect>>;

struct TaggedFieldStruct {
    ExternalTagged ext;
    InternalTagged in;
    AdjacentTagged adj;
};

struct TaggedVariantStruct {
    annotate<TaggedTag>::type<std::variant<int, std::string>> tv;
};

}  // namespace kota::meta::fixtures
