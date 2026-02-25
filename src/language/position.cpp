#include "eventide/language/position.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

namespace {

// Decodes one UTF-8 code point starting at `index`.
// Returns:
// - first: consumed UTF-8 byte count
// - second: corresponding UTF-16 code unit count
// For invalid/truncated sequences it falls back to {1, 1} so callers can
// keep scanning forward without getting stuck.
std::pair<std::uint32_t, std::uint32_t> next_codepoint_sizes(std::string_view text,
                                                             std::size_t index) {
    // Inspect the leading byte to determine UTF-8 sequence width.
    const auto lead = static_cast<unsigned char>(text[index]);
    std::uint32_t utf8 = 1;
    std::uint32_t utf16 = 1;

    // 0xxxxxxx: ASCII, one UTF-8 byte and one UTF-16 code unit.
    if((lead & 0x80u) == 0u) {
        return {utf8, utf16};
    }

    // 110xxxxx: 2-byte UTF-8 sequence -> one UTF-16 code unit.
    if((lead & 0xE0u) == 0xC0u) {
        utf8 = 2;
        // 1110xxxx: 3-byte UTF-8 sequence -> one UTF-16 code unit.
    } else if((lead & 0xF0u) == 0xE0u) {
        utf8 = 3;
        // 11110xxx: 4-byte UTF-8 sequence -> UTF-16 surrogate pair (2 units).
    } else if((lead & 0xF8u) == 0xF0u) {
        utf8 = 4;
        utf16 = 2;
    } else {
        // Invalid leading byte pattern. Consume one byte conservatively.
        return {1, 1};
    }

    // Guard against truncated multi-byte sequences at end of input.
    // We still consume a single byte to guarantee forward progress.
    if(index + utf8 > text.size()) {
        return {1, 1};
    }

    // Valid leading-byte classification and enough bytes available.
    return {utf8, utf16};
}

}  // namespace

namespace eventide::language {

PositionEncoding parse_position_encoding(std::string_view encoding) {
    if(encoding == protocol::PositionEncodingKind::utf8) {
        return PositionEncoding::UTF8;
    }

    if(encoding == protocol::PositionEncodingKind::utf32) {
        return PositionEncoding::UTF32;
    }

    return PositionEncoding::UTF16;
}

PositionMapper::PositionMapper(std::string_view content, PositionEncoding encoding) :
    content(content), encoding(encoding) {
    line_starts.push_back(0);
    for(std::uint32_t i = 0; i < content.size(); ++i) {
        if(content[i] == '\n') {
            line_starts.push_back(i + 1);
        }
    }
}

std::uint32_t PositionMapper::line_of(std::uint32_t offset) const {
    assert(offset <= content.size() && "offset out of range");
    auto it = std::upper_bound(line_starts.begin(), line_starts.end(), offset);
    if(it == line_starts.begin()) {
        return 0;
    }
    return static_cast<std::uint32_t>((it - line_starts.begin()) - 1);
}

std::uint32_t PositionMapper::line_start(std::uint32_t line) const {
    assert(line < line_starts.size() && "line out of range");
    return line_starts[line];
}

std::uint32_t PositionMapper::line_end_exclusive(std::uint32_t line) const {
    assert(line < line_starts.size() && "line out of range");
    if(line + 1 < line_starts.size()) {
        return line_starts[line + 1] - 1;
    }
    return static_cast<std::uint32_t>(content.size());
}

std::uint32_t PositionMapper::measure(std::string_view text) const {
    if(encoding == PositionEncoding::UTF8) {
        return static_cast<std::uint32_t>(text.size());
    }

    std::uint32_t units = 0;
    for(std::size_t index = 0; index < text.size();) {
        auto [utf8, utf16] = next_codepoint_sizes(text, index);
        index += utf8;
        units += (encoding == PositionEncoding::UTF16) ? utf16 : 1;
    }
    return units;
}

std::uint32_t PositionMapper::character(std::uint32_t line, std::uint32_t byte_column) const {
    auto start = line_start(line);
    auto end = line_end_exclusive(line);
    assert(start + byte_column <= end && "byte column out of range");
    return measure(content.substr(start, byte_column));
}

std::uint32_t PositionMapper::length(std::uint32_t line,
                                     std::uint32_t begin_byte_column,
                                     std::uint32_t end_byte_column) const {
    auto start = line_start(line);
    auto end = line_end_exclusive(line);
    assert(start + begin_byte_column <= end && "begin byte column out of range");
    assert(start + end_byte_column <= end && "end byte column out of range");

    if(end_byte_column <= begin_byte_column) {
        return 0;
    }

    auto size = end_byte_column - begin_byte_column;
    return measure(content.substr(start + begin_byte_column, size));
}

protocol::Position PositionMapper::to_position(std::uint32_t offset) const {
    auto line = line_of(offset);
    auto column = offset - line_start(line);
    return protocol::Position{
        .line = line,
        .character = character(line, column),
    };
}

std::uint32_t PositionMapper::to_offset(protocol::Position position) const {
    auto line = position.line;
    auto target = position.character;
    auto begin = line_start(line);
    auto end = line_end_exclusive(line);

    if(target == 0) {
        return begin;
    }

    if(encoding == PositionEncoding::UTF8) {
        assert(begin + target <= end && "character out of range");
        return begin + target;
    }

    std::uint32_t offset = begin;
    auto text = content.substr(begin, end - begin);
    for(std::size_t index = 0; index < text.size();) {
        auto [utf8, utf16] = next_codepoint_sizes(text, index);
        auto step = (encoding == PositionEncoding::UTF16) ? utf16 : 1;
        assert(target >= step && "character out of range");
        target -= step;
        offset += utf8;
        index += utf8;
        if(target == 0) {
            return offset;
        }
    }

    assert(false && "character out of range");
    return end;
}

}  // namespace eventide::language
