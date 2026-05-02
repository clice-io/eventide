#pragma once

#include <cstdint>
#include <string_view>

#include "kota/codec/detail/error.h"

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

using error = kota::codec::serde_error<error_kind>;

}  // namespace kota::codec::toml
