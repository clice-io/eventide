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

using toml_table = ::toml::table;
using toml_array = ::toml::array;
using toml_node = ::toml::node;

enum class error_kind : std::uint16_t {
    ok = 0,
    invalid_state,
    parse_error,
    type_mismatch,
    number_out_of_range,
    unsupported_type,
    trailing_content,
    unknown,
};

constexpr auto error_message(error_kind error) noexcept -> std::string_view {
    switch(error) {
        case error_kind::ok: return "success";
        case error_kind::invalid_state: return "invalid state";
        case error_kind::parse_error: return "parse error";
        case error_kind::type_mismatch: return "type mismatch";
        case error_kind::number_out_of_range: return "number out of range";
        case error_kind::unsupported_type: return "unsupported type";
        case error_kind::trailing_content: return "trailing content";
        case error_kind::unknown:
        default: return "unknown toml error";
    }
}

using error = rich_error;

namespace detail {

constexpr inline std::string_view boxed_root_key = "__value";

}  // namespace detail

}  // namespace kota::codec::toml
