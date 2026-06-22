#include "kota/ipc/lsp/position.h"

#include <algorithm>
#include <cassert>

namespace kota::ipc::lsp {

LineMap::LineMap(std::string_view content,
                 std::span<const std::uint32_t> line_starts,
                 PositionEncoding encoding) : source(content), enc(encoding) {
    if(line_starts.empty()) {
        storage = build_line_starts(content);
        starts = storage;
    } else {
        starts = line_starts;
    }
}

LineMap::LineMap(LineMap&& other) noexcept :
    source(other.source), storage(std::move(other.storage)), starts(other.starts), enc(other.enc) {
    if(!storage.empty()) {
        starts = storage;
    }
}

LineMap& LineMap::operator=(LineMap&& other) noexcept {
    source = other.source;
    storage = std::move(other.storage);
    starts = other.starts;
    enc = other.enc;
    if(!storage.empty()) {
        starts = storage;
    }
    return *this;
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
    auto line = position.line;

    if(line >= starts.size()) [[unlikely]] {
        return std::nullopt;
    }

    auto begin = starts[line];
    std::uint32_t end;
    if(line + 1 < starts.size()) [[likely]] {
        end = starts[line + 1] - 1;
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
    assert(!starts.empty() && "line_starts must not be empty");
    auto it = std::upper_bound(starts.begin(), starts.end(), offset);
    auto line = (it == starts.begin()) ? std::uint32_t{0}
                                       : static_cast<std::uint32_t>((it - starts.begin()) - 1);
    auto start = starts[line];
    std::uint32_t end;
    if(line + 1 < starts.size()) [[likely]] {
        end = starts[line + 1] - 1;
    } else {
        end = static_cast<std::uint32_t>(source.size());
    }
    return {line, start, end};
}

std::string_view LineMap::content() const {
    return source;
}

std::span<const std::uint32_t> LineMap::line_starts() const {
    return starts;
}

}  // namespace kota::ipc::lsp
