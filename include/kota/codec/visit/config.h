#pragma once

#include <string>
#include <string_view>
#include <type_traits>

namespace kota::codec {

/// How enums map to serialized form.
enum class enum_repr { Integer, String };

/// How NaN/Infinity float values are serialized.
enum class nan_repr { Passthrough, Null, String, Error };

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
    /// Enum serialized representation.
    KOTA_CFG_FIELD_(enum_repr, kota::codec::enum_repr::Integer);

    /// NaN/Infinity handling.
    KOTA_CFG_FIELD_(nan_repr, kota::codec::nan_repr::Passthrough);

    /// Deserialize: reject unknown fields in data-driven mode.
    KOTA_CFG_FIELD_(deny_unknown_fields, false);

    /// Generate error path tracking code (prepend_field/prepend_index).
    KOTA_CFG_FIELD_(detailed_error, true);
};

#undef KOTA_CFG_FIELD_

/// True when the visitor computes the output layout statically (flatbuffers).
/// Such backends cannot carry a meta::dynamic repr: there is no layout to
/// compute.
template <typename Vis>
constexpr bool is_layout_computed() {
    if constexpr(requires { Vis::layout_computed; }) {
        return Vis::layout_computed;
    } else {
        return false;
    }
}

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
std::string apply_enum_rename(bool is_serialize, std::string_view name) {
    if constexpr(requires { typename Config::enum_rename; }) {
        return typename Config::enum_rename{}(is_serialize, name);
    } else {
        return std::string(name);
    }
}

}  // namespace kota::codec
