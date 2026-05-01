#pragma once

#include <cstdint>
#include <string_view>

#include "kota/codec/detail/error.h"

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
        case error_kind::InvalidState: return "invalid_state";
        case error_kind::UnexpectedEof: return "unexpected_eof";
        case error_kind::TypeMismatch: return "type mismatch";
        case error_kind::NumberOutOfRange: return "number_out_of_range";
        case error_kind::TrailingBytes: return "trailing_bytes";
        case error_kind::InvalidVariantIndex: return "invalid_variant_index";
        case error_kind::UnsupportedOperation: return "unsupported_operation";
    }

    return "invalid_state";
}

using error = kota::codec::serde_error<error_kind>;

}  // namespace kota::codec::bincode
