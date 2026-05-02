#pragma once

#include <cstdint>
#include <string_view>

#include "kota/codec/detail/error.h"

#if __has_include(<simdjson.h>)
#include "simdjson.h"
#define KOTA_CODEC_JSON_ERROR_HAS_SIMDJSON 1
#else
#define KOTA_CODEC_JSON_ERROR_HAS_SIMDJSON 0
#endif

namespace kota::codec::json {

enum class error_kind : std::uint16_t {
    Ok = 0,
    InvalidState,
    ParseError,
    WriteFailed,
    AllocationFailed,
    TypeMismatch,
    NumberOutOfRange,
    TrailingContent,
    IoError,
    TapeError,
    IndexOutOfBounds,
    NoSuchField,
    AlreadyExists,
    Unknown,
};

constexpr auto error_message(error_kind error) noexcept -> std::string_view {
    switch(error) {
        case error_kind::Ok: return "success";
        case error_kind::InvalidState: return "invalid state";
        case error_kind::ParseError: return "parse error";
        case error_kind::WriteFailed: return "write failed";
        case error_kind::AllocationFailed: return "allocation failed";
        case error_kind::TypeMismatch: return "type mismatch";
        case error_kind::NumberOutOfRange: return "number out of range";
        case error_kind::TrailingContent: return "trailing content";
        case error_kind::IoError: return "I/O error";
        case error_kind::TapeError: return "tape error";
        case error_kind::IndexOutOfBounds: return "index out of bounds";
        case error_kind::NoSuchField: return "no such field";
        case error_kind::AlreadyExists: return "already exists";
        case error_kind::Unknown:
        default: return "unknown json error";
    }
}

#if KOTA_CODEC_JSON_ERROR_HAS_SIMDJSON
constexpr auto make_error(simdjson::error_code error) noexcept -> error_kind {
    switch(error) {
        case simdjson::SUCCESS: return error_kind::Ok;
        case simdjson::MEMALLOC: return error_kind::AllocationFailed;
        case simdjson::INCORRECT_TYPE: return error_kind::TypeMismatch;
        case simdjson::NUMBER_OUT_OF_RANGE: return error_kind::NumberOutOfRange;
        case simdjson::TRAILING_CONTENT: return error_kind::TrailingContent;
        case simdjson::IO_ERROR: return error_kind::IoError;
        case simdjson::INDEX_OUT_OF_BOUNDS: return error_kind::IndexOutOfBounds;
        case simdjson::NO_SUCH_FIELD: return error_kind::NoSuchField;
        case simdjson::TAPE_ERROR: return error_kind::TapeError;
        default: return error_kind::Unknown;
    }
}

#endif

using error = kota::codec::serde_error<error_kind>;

}  // namespace kota::codec::json
