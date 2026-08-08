#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

#include "kota/codec/visit/context.h"

namespace kota::codec::bincode {

enum class error_kind : std::uint8_t {
    UnexpectedEof,
    TypeMismatch,
    NumberOutOfRange,
    TrailingBytes,
};

constexpr std::string_view error_message(error_kind error) {
    switch(error) {
        case error_kind::UnexpectedEof: return "unexpected eof";
        case error_kind::TypeMismatch: return "type mismatch";
        case error_kind::NumberOutOfRange: return "number out of range";
        case error_kind::TrailingBytes: return "trailing bytes";
    }

    std::unreachable();
}

using error = rich_error;

}  // namespace kota::codec::bincode
