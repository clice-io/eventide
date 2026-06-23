#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "kota/ipc/lsp/protocol.h"
#include "kota/ipc/lsp/text.h"

namespace kota::ipc::lsp {

/// Source content + line starts for LSP position conversion.
/// Line starts are held either as a borrowed span or as an owned vector.
class LineMap {
public:
    struct LineBounds {
        std::uint32_t line;
        std::uint32_t start;
        std::uint32_t end;
    };

    /// Compute line starts from content.
    explicit LineMap(std::string_view content, PositionEncoding encoding = PositionEncoding::UTF16);

    /// Borrow pre-computed line starts. Caller must keep the data alive.
    LineMap(std::string_view content,
            std::span<const std::uint32_t> line_starts,
            PositionEncoding encoding = PositionEncoding::UTF16);

    /// Take ownership of pre-computed line starts.
    LineMap(std::string_view content,
            std::vector<std::uint32_t>&& line_starts,
            PositionEncoding encoding = PositionEncoding::UTF16);

    std::optional<protocol::Position>
        to_position(std::uint32_t offset,
                    PositionEncoding encoding = PositionEncoding::Default) const;

    std::optional<std::uint32_t>
        to_offset(protocol::Position position,
                  PositionEncoding encoding = PositionEncoding::Default) const;

    std::optional<protocol::Range>
        to_range(std::uint32_t begin,
                 std::uint32_t end,
                 PositionEncoding encoding = PositionEncoding::Default) const;

    LineBounds line_bounds(std::uint32_t offset) const;

    std::string_view content() const;

    std::span<const std::uint32_t> line_starts() const;

private:
    PositionEncoding resolve(PositionEncoding encoding) const;

    std::string_view source;
    std::variant<std::vector<std::uint32_t>, std::span<const std::uint32_t>> starts;
    PositionEncoding enc;
};

}  // namespace kota::ipc::lsp
