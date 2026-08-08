#pragma once

// Schema-attr fixtures — rename / skip / alias / flatten / default /
// deny_unknown_fields.

#include <string>

#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/codec/macro.h"

namespace kota::meta::fixtures {

struct AnnotatedStruct {
    KOTATSU_ANNOTATE(rename = "id")<int> user_id;
    KOTATSU_ANNOTATE(skip = true)<std::string> internal;
    float value;
};

struct AliasStruct {
    KOTATSU_ANNOTATE(alias = {"user_id", "userId"})<int> id;
    std::string name;
};

struct Inner {
    int a;
    int b;
};

struct Outer {
    int x;
    KOTATSU_ANNOTATE(flatten = true)<Inner> inner;
    int y;
};

struct FlattenTailStruct {
    int head;
    int neck;
    KOTATSU_ANNOTATE(flatten = true)<Inner> body;
};

struct DeepInner {
    int p;
    int q;
};

struct Middle {
    int m;
    KOTATSU_ANNOTATE(flatten = true)<DeepInner> deep;
};

struct DeepOuter {
    int head;
    KOTATSU_ANNOTATE(flatten = true)<Middle> mid;
    int tail;
};

struct FlattenInnerWithSkip {
    int keep_a;
    KOTATSU_ANNOTATE(skip = true)<int> drop_b;
    int keep_c;
};

struct FlattenOuterWithChildSkip {
    int head;
    KOTATSU_ANNOTATE(flatten = true)<FlattenInnerWithSkip> inner;
};

struct FlattenInnerWithRename {
    KOTATSU_ANNOTATE(rename = "renamed_a")<int> a;
    int b;
};

struct FlattenOuterWithChildRename {
    KOTATSU_ANNOTATE(flatten = true)<FlattenInnerWithRename> inner;
};

struct DefaultLiteralStruct {
    KOTATSU_ANNOTATE(defaulted = true)<int> with_default;
    std::string version;
    int plain;
};

struct RenameTarget {
    int user_name;
    std::string display_name;
};

using RenamedRoot = annotation<RenameTarget, attrs::rename_all<rename_policy::lower_camel>>;
using StrictRoot = annotation<RenameTarget, attrs::deny_unknown_fields>;

}  // namespace kota::meta::fixtures
