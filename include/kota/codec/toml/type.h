#pragma once

#include <cstdint>
#include <string_view>

#include "kota/codec/visit/context.h"

#if __has_include(<toml++/toml.hpp>)
#include "toml++/toml.hpp"
#else
#error "toml++/toml.hpp not found."
#endif

namespace kota::codec::toml {

enum class error_kind : std::uint16_t {
    Ok = 0,
    InvalidState,
    ParseError,
    TypeMismatch,
    NumberOutOfRange,
    UnsupportedType,
    TrailingContent,
    Unknown,
};

constexpr auto error_message(error_kind error) noexcept -> std::string_view {
    switch(error) {
        case error_kind::Ok: return "success";
        case error_kind::InvalidState: return "invalid state";
        case error_kind::ParseError: return "parse error";
        case error_kind::TypeMismatch: return "type mismatch";
        case error_kind::NumberOutOfRange: return "number out of range";
        case error_kind::UnsupportedType: return "unsupported type";
        case error_kind::TrailingContent: return "trailing content";
        case error_kind::Unknown:
        default: return "unknown toml error";
    }
}

using Table = ::toml::table;
using Array = ::toml::array;
using Node = ::toml::node;

using error = rich_error;

namespace detail {

constexpr inline std::string_view boxed_root_key = "__value";

}  // namespace detail

}  // namespace kota::codec::toml
