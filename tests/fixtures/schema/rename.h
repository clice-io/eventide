#pragma once

// rename_all Config fixtures — one per built-in policy plus identity.

#include <string>

#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/codec/macro.h"

namespace kota::meta::fixtures {

struct CamelConfig {
    using field_rename = rename_policy::lower_camel;
};

struct PascalConfig {
    using field_rename = rename_policy::upper_camel;
};

struct UpperSnakeConfig {
    using field_rename = rename_policy::upper_snake;
};

struct LowerSnakeConfig {
    using field_rename = rename_policy::lower_snake;
};

struct IdentityConfig {
    using field_rename = rename_policy::identity;
};

struct RenameAllTarget {
    int user_name;
    float total_score;
    std::string item_id;
};

struct MixedRenameStruct {
    KOTATSU_ANNOTATE(rename = "ID")
    <int> user_id;
    float total_score;
    std::string item_name;
};

struct AliasRenameAllStruct {
    KOTATSU_ANNOTATE(alias = {"user_id"})
    <int> id;
    float total_score;
};

}  // namespace kota::meta::fixtures
