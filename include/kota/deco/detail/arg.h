#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kota/support/small_vector.h"

namespace kota::option {

enum class Kind : uint8_t {
    Group,
    Input,
    Unknown,
    Flag,
    Joined,
    Values,
    Separate,
    CommaJoined,
    MultiArg,
    JoinedOrSeparate,
    JoinedAndSeparate,
    RemainingArgs,
    RemainingArgsJoined,
};

enum class RenderStyle : uint8_t {
    CommaJoined,
    Joined,
    Separate,
    Values,
};

enum DriverFlag {
    HelpHidden = (1 << 0),
    RenderAsInput = (1 << 1),
    RenderJoined = (1 << 2),
    RenderSeparate = (1 << 3)
};

enum DriverVisibility {
    DefaultVis = (1 << 0),
};

struct ParseError {
    std::uint32_t index;
    const char* message;
};

/// Values (values) are non-owning views into the argv data passed to OptTable::parse().
struct ParsedArg {
    std::uint32_t id = 0;

    std::uint32_t index = 0;
    std::uint32_t next_index = 0;

    std::string spelling;

    kota::small_vector<std::string_view, 2> values;

    void add_value(std::string_view v) {
        values.push_back(v);
    }

    void clear() {
        id = 0;
        index = 0;
        next_index = 0;
        values.clear();
        spelling.clear();
    }
};

constexpr inline std::string_view _pfx_dash[] = {"-"};
constexpr inline std::string_view _pfx_dash_double[] = {"-", "--"};
constexpr inline std::string_view _pfx_double[] = {"--"};
constexpr inline std::string_view _pfx_all[] = {"--", "/", "-"};
constexpr inline std::string_view _pfx_slash_dash[] = {"/", "-"};

constexpr inline auto pfx_none = std::span<const std::string_view>();
constexpr inline auto pfx_dash = std::span<const std::string_view>(_pfx_dash);
constexpr inline auto pfx_dash_double = std::span<const std::string_view>(_pfx_dash_double);
constexpr inline auto pfx_double = std::span<const std::string_view>(_pfx_double);
constexpr inline auto pfx_all = std::span<const std::string_view>(_pfx_all);
constexpr inline auto pfx_slash_dash = std::span<const std::string_view>(_pfx_slash_dash);

}  // namespace kota::option
