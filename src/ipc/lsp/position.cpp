#include "kota/ipc/lsp/position.h"

#include <algorithm>
#include <cassert>

namespace kota::ipc::lsp {

LineMap::LineMap(std::string_view content, PositionEncoding encoding) :
    source(content), starts(build_line_starts(content)), enc(encoding) {
    assert(encoding != PositionEncoding::Default &&
           "Default is not valid for LineMap construction");
}

LineMap::LineMap(std::string_view content,
                 std::span<const std::uint32_t> line_starts,
                 PositionEncoding encoding) : source(content), starts(line_starts), enc(encoding) {
    assert(encoding != PositionEncoding::Default &&
           "Default is not valid for LineMap construction");
}

LineMap::LineMap(std::string_view content,
                 std::vector<std::uint32_t>&& line_starts,
                 PositionEncoding encoding) :
    source(content), starts(std::move(line_starts)), enc(encoding) {
    assert(encoding != PositionEncoding::Default &&
           "Default is not valid for LineMap construction");
}

PositionEncoding LineMap::resolve(PositionEncoding encoding) const {
    return encoding == PositionEncoding::Default ? enc : encoding;
}

std::optional<protocol::Position> LineMap::to_position(std::uint32_t offset,
                                                       PositionEncoding encoding) const {
    auto actual = resolve(encoding);
    if(offset > source.size()) [[unlikely]] {
        return std::nullopt;
    }
    auto bounds = line_bounds(offset);
    auto column = offset - bounds.start;
    if(bounds.start + column > bounds.end) [[unlikely]] {
        return std::nullopt;
    }
    return protocol::Position{
        .line = bounds.line,
        .character = encoded_length(source.substr(bounds.start, column), actual),
    };
}

std::optional<std::uint32_t> LineMap::to_offset(protocol::Position position,
                                                PositionEncoding encoding) const {
    auto actual = resolve(encoding);
    auto ls = line_starts();
    auto line = position.line;

    if(line >= ls.size()) [[unlikely]] {
        return std::nullopt;
    }

    auto begin = ls[line];
    std::uint32_t end;
    if(line + 1 < ls.size()) [[likely]] {
        end = ls[line + 1] - 1;
    } else {
        end = static_cast<std::uint32_t>(source.size());
    }

    auto result = encoded_offset(source.substr(begin, end - begin), position.character, actual);
    if(!result) {
        return std::nullopt;
    }
    return begin + *result;
}

std::optional<protocol::Range> LineMap::to_range(std::uint32_t begin,
                                                 std::uint32_t end,
                                                 PositionEncoding encoding) const {
    if(begin > end) [[unlikely]] {
        return std::nullopt;
    }
    auto start = to_position(begin, encoding);
    if(!start) {
        return std::nullopt;
    }
    auto stop = to_position(end, encoding);
    if(!stop) {
        return std::nullopt;
    }
    return protocol::Range{.start = *start, .end = *stop};
}

LineMap::LineBounds LineMap::line_bounds(std::uint32_t offset) const {
    auto ls = line_starts();
    assert(!ls.empty() && "line_starts must not be empty");
    auto it = std::upper_bound(ls.begin(), ls.end(), offset);
    auto line =
        (it == ls.begin()) ? std::uint32_t{0} : static_cast<std::uint32_t>((it - ls.begin()) - 1);
    auto start = ls[line];
    std::uint32_t end;
    if(line + 1 < ls.size()) [[likely]] {
        end = ls[line + 1] - 1;
    } else {
        end = static_cast<std::uint32_t>(source.size());
    }
    return {line, start, end};
}

std::string_view LineMap::content() const {
    return source;
}

std::span<const std::uint32_t> LineMap::line_starts() const {
    return std::visit([](const auto& v) -> std::span<const std::uint32_t> { return v; }, starts);
}

}  // namespace kota::ipc::lsp
