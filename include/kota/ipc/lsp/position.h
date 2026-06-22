#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "kota/ipc/lsp/protocol.h"
#include "kota/ipc/lsp/text.h"

namespace kota::ipc::lsp {

/// Non-owning view over source content + line starts for LSP position conversion.
/// When `line_starts` is not provided at construction, it is computed eagerly
/// and stored internally. Move operations fix up the internal span.
class LineMap {
public:
    struct LineBounds {
        std::uint32_t line;
        std::uint32_t start;
        std::uint32_t end;
    };

    explicit LineMap(std::string_view content,
                     std::span<const std::uint32_t> line_starts = {},
                     PositionEncoding encoding = PositionEncoding::UTF16);

    LineMap(LineMap&& other) noexcept;

    LineMap& operator=(LineMap&& other) noexcept;

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
    std::vector<std::uint32_t> storage;
    std::span<const std::uint32_t> starts;
    PositionEncoding enc;
};

}  // namespace kota::ipc::lsp
