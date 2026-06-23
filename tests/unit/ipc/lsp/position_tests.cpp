#include <cstdint>

#include "kota/zest/zest.h"
#include "kota/ipc/lsp/position.h"

namespace kota::ipc::lsp {
namespace {

TEST_SUITE(language_position) {

TEST_CASE(utf16_column_counts) {
    std::string_view content = "a\xe4\xbd\xa0" "b\n";
    LineMap map(content, PositionEncoding::UTF16);

    auto position = map.to_position(4);
    ASSERT_TRUE(position.has_value());
    ASSERT_EQ(position->line, 0U);
    ASSERT_EQ(position->character, 2U);
}

TEST_CASE(round_trip_offsets) {
    std::string_view content = "a\xe4\xbd\xa0" "b\nx\xf0\x9f\x99\x82" "y";
    constexpr std::uint32_t offsets[] = {0, 1, 4, 5, 6, 7, 11, 12};

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        LineMap map(content, encoding);
        for(auto offset: offsets) {
            auto position = map.to_position(offset);
            ASSERT_TRUE(position.has_value());
            auto mapped = map.to_offset(*position);
            ASSERT_TRUE(mapped.has_value());
            ASSERT_EQ(*mapped, offset);
        }
    }
}

TEST_CASE(position_offset_values) {
    std::string_view content = "a\xe4\xbd\xa0\xf0\x9f\x99\x82" "b\nx";

    struct Sample {
        std::uint32_t offset;
        std::uint32_t line;
        std::uint32_t utf8_character;
        std::uint32_t utf16_character;
        std::uint32_t utf32_character;
    };

    constexpr Sample samples[] = {
        {.offset = 0,  .line = 0, .utf8_character = 0, .utf16_character = 0, .utf32_character = 0},
        {.offset = 1,  .line = 0, .utf8_character = 1, .utf16_character = 1, .utf32_character = 1},
        {.offset = 4,  .line = 0, .utf8_character = 4, .utf16_character = 2, .utf32_character = 2},
        {.offset = 8,  .line = 0, .utf8_character = 8, .utf16_character = 4, .utf32_character = 3},
        {.offset = 9,  .line = 0, .utf8_character = 9, .utf16_character = 5, .utf32_character = 4},
        {.offset = 10, .line = 1, .utf8_character = 0, .utf16_character = 0, .utf32_character = 0},
        {.offset = 11, .line = 1, .utf8_character = 1, .utf16_character = 1, .utf32_character = 1},
    };

    LineMap map8(content, PositionEncoding::UTF8);
    LineMap map16(content, PositionEncoding::UTF16);
    LineMap map32(content, PositionEncoding::UTF32);

    for(const auto& sample: samples) {
        auto p8 = map8.to_position(sample.offset);
        ASSERT_TRUE(p8.has_value());
        EXPECT_EQ(p8->line, sample.line);
        EXPECT_EQ(p8->character, sample.utf8_character);
        auto o8 = map8.to_offset(*p8);
        ASSERT_TRUE(o8.has_value());
        EXPECT_EQ(*o8, sample.offset);

        auto p16 = map16.to_position(sample.offset);
        ASSERT_TRUE(p16.has_value());
        EXPECT_EQ(p16->line, sample.line);
        EXPECT_EQ(p16->character, sample.utf16_character);
        auto o16 = map16.to_offset(*p16);
        ASSERT_TRUE(o16.has_value());
        EXPECT_EQ(*o16, sample.offset);

        auto p32 = map32.to_position(sample.offset);
        ASSERT_TRUE(p32.has_value());
        EXPECT_EQ(p32->line, sample.line);
        EXPECT_EQ(p32->character, sample.utf32_character);
        auto o32 = map32.to_offset(*p32);
        ASSERT_TRUE(o32.has_value());
        EXPECT_EQ(*o32, sample.offset);
    }
}

TEST_CASE(line_bounds_values) {
    std::string_view content = "ab\n\ncd";
    LineMap map(content);

    auto b0 = map.line_bounds(0);
    EXPECT_EQ(b0.line, 0U);
    EXPECT_EQ(b0.start, 0U);
    EXPECT_EQ(b0.end, 2U);

    auto b1 = map.line_bounds(3);
    EXPECT_EQ(b1.line, 1U);
    EXPECT_EQ(b1.start, 3U);
    EXPECT_EQ(b1.end, 3U);

    auto b2 = map.line_bounds(4);
    EXPECT_EQ(b2.line, 2U);
    EXPECT_EQ(b2.start, 4U);
    EXPECT_EQ(b2.end, 6U);

    EXPECT_EQ(map.line_bounds(2).line, 0U);
    EXPECT_EQ(map.line_bounds(6).line, 2U);
}

TEST_CASE(measure_units_encoding) {
    std::string_view content = "a\xe4\xbd\xa0\xf0\x9f\x99\x82z";

    EXPECT_EQ(encoded_length(content, PositionEncoding::UTF8), 9U);
    EXPECT_EQ(encoded_length(content, PositionEncoding::UTF16), 5U);
    EXPECT_EQ(encoded_length(content, PositionEncoding::UTF32), 4U);
}

TEST_CASE(roundtrip_multiline_boundaries) {
    std::string_view content = "a\xe4\xbd\xa0\n\xf0\x9f\x99\x82" "b";
    constexpr std::uint32_t boundaries[] = {0, 1, 4, 5, 9, 10};

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        LineMap map(content, encoding);
        for(auto offset: boundaries) {
            auto position = map.to_position(offset);
            ASSERT_TRUE(position.has_value());
            auto mapped = map.to_offset(*position);
            ASSERT_TRUE(mapped.has_value());
            ASSERT_EQ(*mapped, offset);
        }
    }
}

TEST_CASE(invalid_continuation_progress) {
    auto expect_progress = [&](auto... bytes) {
        const char raw[] = {static_cast<char>(bytes)...};
        constexpr auto len = static_cast<std::uint32_t>(sizeof...(bytes));
        auto content = std::string_view(raw, sizeof...(bytes));

        EXPECT_EQ(encoded_length(content, PositionEncoding::UTF8), len);
        EXPECT_EQ(encoded_length(content, PositionEncoding::UTF16), len);
        EXPECT_EQ(encoded_length(content, PositionEncoding::UTF32), len);
    };

    // 3-byte lead with invalid second byte.
    expect_progress('a', 0xE4u, 'X', 'b');
    // 2-byte lead with invalid continuation byte.
    expect_progress(0xC2u, 'A');
    // 3-byte lead with invalid third byte.
    expect_progress(0xE1u, 0x80u, 'B');
    // 4-byte lead with invalid second byte.
    expect_progress(0xF1u, 'C', 0x80u, 0x80u);
    // 4-byte lead with invalid fourth byte.
    expect_progress(0xF1u, 0x80u, 0x80u, 'D');
}

TEST_CASE(invalid_position_stability) {
    auto expect_stable = [&](std::string_view content) {
        for(auto encoding:
            {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
            LineMap map(content, encoding);
            for(std::uint32_t offset = 0; offset <= content.size(); ++offset) {
                auto position = map.to_position(offset);
                ASSERT_TRUE(position.has_value());
                auto mapped_offset = map.to_offset(*position);
                ASSERT_TRUE(mapped_offset.has_value());
                EXPECT_TRUE(*mapped_offset <= content.size());
            }
        }
    };

    auto expect_stable_bytes = [&](auto... bytes) {
        const char raw[] = {static_cast<char>(bytes)...};
        expect_stable(std::string_view(raw, sizeof...(bytes)));
    };

    expect_stable_bytes('a', 0xE4u, 'X', 'b');
    expect_stable_bytes('x', 0xF0u, 0x9Fu, '\n', 'y');
    expect_stable_bytes(0xF5u, 0x80u, 0x80u, 0x80u, '\n', 'z');
}

TEST_CASE(strict_utf8_validation) {
    auto expect_invalid_sequence = [&](auto... bytes) {
        const char raw[] = {static_cast<char>(bytes)...};
        constexpr auto len = static_cast<std::uint32_t>(sizeof...(bytes));
        auto content = std::string_view(raw, sizeof...(bytes));

        EXPECT_EQ(encoded_length(content, PositionEncoding::UTF16), len);
        EXPECT_EQ(encoded_length(content, PositionEncoding::UTF32), len);
    };

    expect_invalid_sequence(0xC0u, 0x80u);
    expect_invalid_sequence(0xE0u, 0x80u, 0x80u);
    expect_invalid_sequence(0xEDu, 0xA0u, 0x80u);
    expect_invalid_sequence(0xF4u, 0x90u, 0x80u, 0x80u);
    expect_invalid_sequence(0xF5u, 0x80u, 0x80u, 0x80u);
    expect_invalid_sequence('a', 0xF0u, 0x9Fu, 'b');
}

TEST_CASE(to_position_out_of_range) {
    std::string_view content = "abc\ndef";
    LineMap map(content, PositionEncoding::UTF8);

    EXPECT_FALSE(map.to_position(100).has_value());
    EXPECT_FALSE(map.to_position(8).has_value());
    EXPECT_TRUE(map.to_position(7).has_value());
}

TEST_CASE(to_offset_line_out_of_range) {
    std::string_view content = "abc\ndef";
    LineMap map(content, PositionEncoding::UTF8);

    EXPECT_FALSE(map.to_offset({.line = 5, .character = 0}).has_value());
    EXPECT_FALSE(map.to_offset({.line = 2, .character = 0}).has_value());
    EXPECT_TRUE(map.to_offset({.line = 1, .character = 0}).has_value());
}

TEST_CASE(to_offset_character_out_of_range) {
    std::string_view content = "abc\ndef";

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        LineMap map(content, encoding);

        EXPECT_FALSE(map.to_offset({.line = 0, .character = 10}).has_value());
        EXPECT_FALSE(map.to_offset({.line = 1, .character = 4}).has_value());
        EXPECT_TRUE(map.to_offset({.line = 0, .character = 3}).has_value());
        EXPECT_TRUE(map.to_offset({.line = 1, .character = 3}).has_value());
    }
}

TEST_CASE(encoding_override) {
    std::string_view content = "a\xe4\xbd\xa0" "b\n";
    LineMap map(content, PositionEncoding::UTF8);

    auto p_default = map.to_position(4);
    ASSERT_TRUE(p_default.has_value());
    EXPECT_EQ(p_default->character, 4U);

    auto p_utf16 = map.to_position(4, PositionEncoding::UTF16);
    ASSERT_TRUE(p_utf16.has_value());
    EXPECT_EQ(p_utf16->character, 2U);
}

TEST_CASE(to_range_basic) {
    std::string_view content = "abc\ndef";
    LineMap map(content, PositionEncoding::UTF8);

    auto range = map.to_range(0, 3);
    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->start.line, 0U);
    EXPECT_EQ(range->start.character, 0U);
    EXPECT_EQ(range->end.line, 0U);
    EXPECT_EQ(range->end.character, 3U);

    auto cross_line = map.to_range(0, 5);
    ASSERT_TRUE(cross_line.has_value());
    EXPECT_EQ(cross_line->start.line, 0U);
    EXPECT_EQ(cross_line->end.line, 1U);
    EXPECT_EQ(cross_line->end.character, 1U);
}

TEST_CASE(borrowed_line_starts) {
    std::string_view content = "ab\ncd";
    auto starts = build_line_starts(content);
    LineMap map(content, std::span<const std::uint32_t>(starts), PositionEncoding::UTF8);

    EXPECT_EQ(map.line_starts().data(), starts.data());
    EXPECT_EQ(map.line_starts().size(), starts.size());

    auto p = map.to_position(3);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->line, 1U);
    EXPECT_EQ(p->character, 0U);
}

TEST_CASE(move_semantics) {
    std::string_view content = "ab\ncd";
    LineMap map(content, PositionEncoding::UTF8);

    LineMap moved(std::move(map));
    auto p = moved.to_position(3);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->line, 1U);
    EXPECT_EQ(p->character, 0U);

    LineMap assigned(std::string_view("x"));
    assigned = std::move(moved);
    auto p2 = assigned.to_position(4);
    ASSERT_TRUE(p2.has_value());
    EXPECT_EQ(p2->line, 1U);
    EXPECT_EQ(p2->character, 1U);
}

};  // TEST_SUITE(language_position)

}  // namespace
}  // namespace kota::ipc::lsp
