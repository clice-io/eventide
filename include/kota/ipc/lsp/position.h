#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "kota/ipc/lsp/protocol.h"

namespace kota::ipc::lsp {

/// Position unit encoding used by LSP line/character coordinates.
enum class PositionEncoding : std::uint8_t {
    /// Character counts UTF-8 code units (bytes).
    UTF8,

    /// Character counts UTF-16 code units.
    UTF16,

    /// Character counts UTF-32 code units (code points).
    UTF32,
};

/// Parses LSP encoding name (e.g. "utf-16") to `PositionEncoding`.
/// Unknown values fall back to `PositionEncoding::UTF16`.
PositionEncoding parse_position_encoding(std::string_view encoding);

/// Scans `content` for newlines and returns byte offsets of each line start.
std::vector<std::uint32_t> build_line_starts(std::string_view content);

/// Byte range of a single line within the content.
struct LineBounds {
    /// Zero-based line number.
    std::uint32_t line;
    /// Byte offset of the first character on this line.
    std::uint32_t start;
    /// Byte offset one past the last content character (excluding '\n').
    std::uint32_t end;
};

/// Returns the line number and byte range `[start, end)` for the line containing `offset`.
/// `end` points one past the last content byte (excluding the '\n').
LineBounds line_bounds(std::span<const std::uint32_t> line_starts,
                       std::uint32_t bound,
                       std::uint32_t offset);

/// Returns `text` length in the given position encoding.
std::uint32_t encoded_length(std::string_view text, PositionEncoding encoding);

/// Converts a byte offset to LSP `Position{line, character}`.
/// Returns `std::nullopt` when the offset is out of range.
std::optional<protocol::Position> to_position(std::string_view content,
                                              std::span<const std::uint32_t> line_starts,
                                              PositionEncoding encoding,
                                              std::uint32_t offset);

/// Converts LSP position to byte offset in the original text.
/// Returns `std::nullopt` when the position is out of range.
std::optional<std::uint32_t> to_offset(std::string_view content,
                                       std::span<const std::uint32_t> line_starts,
                                       PositionEncoding encoding,
                                       protocol::Position position);

}  // namespace kota::ipc::lsp
