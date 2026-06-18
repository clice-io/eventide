#include <cstdint>

#include "kota/zest/zest.h"
#include "kota/ipc/lsp/position.h"

namespace kota::ipc::lsp {
namespace {

TEST_SUITE(language_position) {

TEST_CASE(parse_encoding_values) {
    EXPECT_EQ(parse_position_encoding(protocol::PositionEncodingKind::utf8),
              PositionEncoding::UTF8);
    EXPECT_EQ(parse_position_encoding(protocol::PositionEncodingKind::utf16),
              PositionEncoding::UTF16);
    EXPECT_EQ(parse_position_encoding(protocol::PositionEncodingKind::utf32),
              PositionEncoding::UTF32);
    EXPECT_EQ(parse_position_encoding("unknown-encoding"), PositionEncoding::UTF16);
}

TEST_CASE(utf16_column_counts) {
    std::string_view content = "a\xe4\xbd\xa0" "b\n";
    auto starts = build_line_starts(content);

    auto position = to_position(content, starts, PositionEncoding::UTF16, 4);
    ASSERT_TRUE(position.has_value());
    ASSERT_EQ(position->line, 0U);
    ASSERT_EQ(position->character, 2U);
}

TEST_CASE(round_trip_offsets) {
    std::string_view content = "a\xe4\xbd\xa0" "b\nx\xf0\x9f\x99\x82" "y";
    constexpr std::uint32_t offsets[] = {0, 1, 4, 5, 6, 7, 11, 12};
    auto starts = build_line_starts(content);

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        for(auto offset: offsets) {
            auto position = to_position(content, starts, encoding, offset);
            ASSERT_TRUE(position.has_value());
            auto mapped = to_offset(content, starts, encoding, *position);
            ASSERT_TRUE(mapped.has_value());
            ASSERT_EQ(*mapped, offset);
        }
    }
}

TEST_CASE(position_offset_values) {
    std::string_view content = "a\xe4\xbd\xa0\xf0\x9f\x99\x82" "b\nx";
    auto starts = build_line_starts(content);

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

    for(const auto& sample: samples) {
        auto p8 = to_position(content, starts, PositionEncoding::UTF8, sample.offset);
        ASSERT_TRUE(p8.has_value());
        EXPECT_EQ(p8->line, sample.line);
        EXPECT_EQ(p8->character, sample.utf8_character);
        auto o8 = to_offset(content, starts, PositionEncoding::UTF8, *p8);
        ASSERT_TRUE(o8.has_value());
        EXPECT_EQ(*o8, sample.offset);

        auto p16 = to_position(content, starts, PositionEncoding::UTF16, sample.offset);
        ASSERT_TRUE(p16.has_value());
        EXPECT_EQ(p16->line, sample.line);
        EXPECT_EQ(p16->character, sample.utf16_character);
        auto o16 = to_offset(content, starts, PositionEncoding::UTF16, *p16);
        ASSERT_TRUE(o16.has_value());
        EXPECT_EQ(*o16, sample.offset);

        auto p32 = to_position(content, starts, PositionEncoding::UTF32, sample.offset);
        ASSERT_TRUE(p32.has_value());
        EXPECT_EQ(p32->line, sample.line);
        EXPECT_EQ(p32->character, sample.utf32_character);
        auto o32 = to_offset(content, starts, PositionEncoding::UTF32, *p32);
        ASSERT_TRUE(o32.has_value());
        EXPECT_EQ(*o32, sample.offset);
    }
}

TEST_CASE(line_bounds_values) {
    std::string_view content = "ab\n\ncd";
    auto starts = build_line_starts(content);
    auto size = static_cast<std::uint32_t>(content.size());

    auto b0 = line_bounds(starts, size, 0);
    EXPECT_EQ(b0.line, 0U);
    EXPECT_EQ(b0.start, 0U);
    EXPECT_EQ(b0.end, 2U);

    auto b1 = line_bounds(starts, size, 3);
    EXPECT_EQ(b1.line, 1U);
    EXPECT_EQ(b1.start, 3U);
    EXPECT_EQ(b1.end, 3U);

    auto b2 = line_bounds(starts, size, 4);
    EXPECT_EQ(b2.line, 2U);
    EXPECT_EQ(b2.start, 4U);
    EXPECT_EQ(b2.end, 6U);

    EXPECT_EQ(line_bounds(starts, size, 2).line, 0U);
    EXPECT_EQ(line_bounds(starts, size, 6).line, 2U);
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
    auto starts = build_line_starts(content);

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        for(auto offset: boundaries) {
            auto position = to_position(content, starts, encoding, offset);
            ASSERT_TRUE(position.has_value());
            auto mapped = to_offset(content, starts, encoding, *position);
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
        auto starts = build_line_starts(content);
        for(auto encoding:
            {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
            for(std::uint32_t offset = 0; offset <= content.size(); ++offset) {
                auto position = to_position(content, starts, encoding, offset);
                ASSERT_TRUE(position.has_value());
                auto mapped_offset = to_offset(content, starts, encoding, *position);
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
    auto starts = build_line_starts(content);

    // Offset beyond content size.
    EXPECT_FALSE(to_position(content, starts, PositionEncoding::UTF8, 100).has_value());
    EXPECT_FALSE(to_position(content, starts, PositionEncoding::UTF8, 8).has_value());

    // Offset at content size is valid (EOF position).
    EXPECT_TRUE(to_position(content, starts, PositionEncoding::UTF8, 7).has_value());
}

TEST_CASE(to_offset_line_out_of_range) {
    std::string_view content = "abc\ndef";
    auto starts = build_line_starts(content);

    // Line beyond document.
    EXPECT_FALSE(to_offset(content, starts, PositionEncoding::UTF8, {.line = 5, .character = 0})
                     .has_value());
    EXPECT_FALSE(to_offset(content, starts, PositionEncoding::UTF8, {.line = 2, .character = 0})
                     .has_value());

    // Valid last line.
    EXPECT_TRUE(to_offset(content, starts, PositionEncoding::UTF8, {.line = 1, .character = 0})
                    .has_value());
}

TEST_CASE(to_offset_character_out_of_range) {
    std::string_view content = "abc\ndef";
    auto starts = build_line_starts(content);

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        // Character beyond line length.
        EXPECT_FALSE(
            to_offset(content, starts, encoding, {.line = 0, .character = 10}).has_value());
        EXPECT_FALSE(to_offset(content, starts, encoding, {.line = 1, .character = 4}).has_value());

        // Valid end of line.
        EXPECT_TRUE(to_offset(content, starts, encoding, {.line = 0, .character = 3}).has_value());
        EXPECT_TRUE(to_offset(content, starts, encoding, {.line = 1, .character = 3}).has_value());
    }
}

};  // TEST_SUITE(language_position)

}  // namespace
}  // namespace kota::ipc::lsp
