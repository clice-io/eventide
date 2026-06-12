#pragma once

#include <cstdint>
#include <string_view>

#include "kota/codec/visit/context.h"

namespace kota::codec::bincode {

enum class error_kind : std::uint8_t {
    Ok = 0,
    InvalidState,
    UnexpectedEof,
    TypeMismatch,
    NumberOutOfRange,
    TrailingBytes,
    InvalidVariantIndex,
    UnsupportedOperation,
};

constexpr std::string_view error_message(error_kind error) {
    switch(error) {
        case error_kind::Ok: return "ok";
        case error_kind::InvalidState: return "invalid state";
        case error_kind::UnexpectedEof: return "unexpected eof";
        case error_kind::TypeMismatch: return "type mismatch";
        case error_kind::NumberOutOfRange: return "number out of range";
        case error_kind::TrailingBytes: return "trailing bytes";
        case error_kind::InvalidVariantIndex: return "invalid variant index";
        case error_kind::UnsupportedOperation: return "unsupported operation";
    }

    return "unknown error";
}

using error = rich_error;

}  // namespace kota::codec::bincode
