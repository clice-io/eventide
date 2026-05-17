#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

#include "kota/support/naming.h"
#include "kota/support/tuple_traits.h"
#include "kota/meta/attrs.h"

namespace kota::codec {

/// How enums map to wire format.
enum class enum_repr { Integer, String };

/// How NaN/Infinity float values are serialized.
enum class nan_repr { Passthrough, Null, String, Error };

/// How chrono time_point values are serialized. No default — must be explicitly chosen.
enum class chrono_repr { Iso8601, EpochSeconds, EpochMillis };

/// What to do when a duplicate key is encountered during data-driven deserialization.
enum class duplicate_keys { LastWins, Error };

namespace spelling {

using identity = kota::naming::rename_policy::identity;
using snake_case = kota::naming::rename_policy::lower_snake;
using camelCase = kota::naming::rename_policy::lower_camel;
using PascalCase = kota::naming::rename_policy::upper_camel;
using SCREAMING_SNAKE_CASE = kota::naming::rename_policy::upper_snake;

}  // namespace spelling

namespace detail {

struct empty_config_base {};

}  // namespace detail

// clang-format off
#define KOTA_CFG_FIELD_(name, default_val)                                                         \
    constexpr static auto name = [] {                                                              \
        if constexpr(std::is_void_v<UserConfig>)                                                   \
            return (default_val);                                                                   \
        else if constexpr(requires { UserConfig::name; })                                          \
            return UserConfig::name;                                                               \
        else                                                                                       \
            return (default_val);                                                                   \
    }()
// clang-format on

/// Config template that resolves missing fields from defaults.
/// `default_config<>` gives all defaults. `default_config<MyConfig>` fills in
/// any fields that MyConfig does not provide.  Inherits from UserConfig so that
/// type aliases (field_rename, enum_rename, etc.) are forwarded transparently.
template <typename UserConfig = void>
struct default_config :
    std::conditional_t<std::is_void_v<UserConfig>, detail::empty_config_base, UserConfig> {
    /// Enum wire representation.
    KOTA_CFG_FIELD_(enum_repr, kota::codec::enum_repr::Integer);

    /// NaN/Infinity handling.
    KOTA_CFG_FIELD_(nan_repr, kota::codec::nan_repr::Passthrough);

    /// Duplicate key policy in data-driven deserialization.
    KOTA_CFG_FIELD_(duplicate_keys, kota::codec::duplicate_keys::LastWins);

    /// Serialize: skip nullable fields (optional/unique_ptr/shared_ptr) when null.
    KOTA_CFG_FIELD_(skip_none_fields, false);

    /// Deserialize: reject unknown fields in data-driven mode.
    KOTA_CFG_FIELD_(deny_unknown_fields, false);

    /// Deserialize: use T{} for missing fields instead of reporting error.
    KOTA_CFG_FIELD_(default_for_missing, false);

    /// Deserialize: check narrowing overflow on numeric conversions.
    KOTA_CFG_FIELD_(strict_numeric, false);

    /// Generate error path tracking code (prepend_field/prepend_index).
    KOTA_CFG_FIELD_(detailed_error, true);

    /// Recursion depth limit (0 = unlimited).
    KOTA_CFG_FIELD_(max_depth, std::size_t(0));

    /// Max string byte length on deserialization (0 = unlimited).
    KOTA_CFG_FIELD_(max_string_length, std::size_t(0));

    /// Max container element count on deserialization (0 = unlimited).
    KOTA_CFG_FIELD_(max_container_size, std::size_t(0));
};

#undef KOTA_CFG_FIELD_

/// Config > Vis > true. Determines text vs binary serialization strategy for user-defined types.
template <typename Config, typename Vis>
constexpr bool is_human_readable() {
    if constexpr(requires { Config::human_readable; }) {
        return Config::human_readable;
    } else if constexpr(requires { Vis::human_readable; }) {
        return Vis::human_readable;
    } else {
        return true;
    }
}

template <typename Config>
std::string apply_field_rename(bool is_serialize, std::string_view name) {
    if constexpr(requires { typename Config::field_rename; }) {
        return typename Config::field_rename{}(is_serialize, name);
    } else {
        return std::string(name);
    }
}

template <typename Config>
std::string apply_enum_rename(bool is_serialize, std::string_view name) {
    if constexpr(requires { typename Config::enum_rename; }) {
        return typename Config::enum_rename{}(is_serialize, name);
    } else {
        return std::string(name);
    }
}

namespace detail {

template <typename BaseConfig,
          typename Attrs,
          bool HasRenameAll = tuple_has_spec_v<Attrs, meta::attrs::rename_all>,
          bool HasDenyUnknown = tuple_has_v<Attrs, meta::attrs::deny_unknown_fields>>
struct annotated_config_impl {
    using type = BaseConfig;
};

template <typename BaseConfig, typename Attrs>
struct annotated_config_impl<BaseConfig, Attrs, true, false> {
    struct type : BaseConfig {
        using field_rename = typename tuple_find_spec_t<Attrs, meta::attrs::rename_all>::policy;
    };
};

template <typename BaseConfig, typename Attrs>
struct annotated_config_impl<BaseConfig, Attrs, false, true> {
    struct type : BaseConfig {
        constexpr static bool deny_unknown_fields = true;
    };
};

template <typename BaseConfig, typename Attrs>
struct annotated_config_impl<BaseConfig, Attrs, true, true> {
    struct type : BaseConfig {
        using field_rename = typename tuple_find_spec_t<Attrs, meta::attrs::rename_all>::policy;
        constexpr static bool deny_unknown_fields = true;
    };
};

template <typename BaseConfig, typename Attrs>
using annotated_config = typename annotated_config_impl<BaseConfig, Attrs>::type;

}  // namespace detail

}  // namespace kota::codec
