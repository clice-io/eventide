#include <cstdint>

#include "eventide/zest/zest.h"
#include "eventide/language/position.h"

namespace eventide::language {

TEST_SUITE(language_position) {

TEST_CASE(parse_position_encoding_values) {
    EXPECT_EQ(parse_position_encoding(protocol::PositionEncodingKind::utf8),
              PositionEncoding::UTF8);
    EXPECT_EQ(parse_position_encoding(protocol::PositionEncodingKind::utf16),
              PositionEncoding::UTF16);
    EXPECT_EQ(parse_position_encoding(protocol::PositionEncodingKind::utf32),
              PositionEncoding::UTF32);
    EXPECT_EQ(parse_position_encoding("unknown-encoding"), PositionEncoding::UTF16);
}

TEST_CASE(utf16_column) {
    std::string_view content = "a\xe4\xbd\xa0" "b\n";
    PositionMapper converter(content, PositionEncoding::UTF16);

    auto position = converter.to_position(4);
    ASSERT_EQ(position.line, 0U);
    ASSERT_EQ(position.character, 2U);
}

TEST_CASE(round_trip_offset) {
    std::string_view content = "a\xe4\xbd\xa0" "b\nx\xf0\x9f\x99\x82" "y";
    constexpr std::uint32_t offsets[] = {0, 1, 4, 5, 6, 7, 11, 12};

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        PositionMapper converter(content, encoding);
        for(auto offset: offsets) {
            auto position = converter.to_position(offset);
            ASSERT_EQ(converter.to_offset(position), offset);
        }
    }
}

TEST_CASE(line_helpers_on_empty_and_nonempty_lines) {
    std::string_view content = "ab\n\ncd";
    PositionMapper converter(content, PositionEncoding::UTF8);

    EXPECT_EQ(converter.line_start(0), 0U);
    EXPECT_EQ(converter.line_end_exclusive(0), 2U);
    EXPECT_EQ(converter.line_start(1), 3U);
    EXPECT_EQ(converter.line_end_exclusive(1), 3U);
    EXPECT_EQ(converter.line_start(2), 4U);
    EXPECT_EQ(converter.line_end_exclusive(2), 6U);

    EXPECT_EQ(converter.line_of(0), 0U);
    EXPECT_EQ(converter.line_of(2), 0U);
    EXPECT_EQ(converter.line_of(3), 1U);
    EXPECT_EQ(converter.line_of(4), 2U);
    EXPECT_EQ(converter.line_of(6), 2U);
}

TEST_CASE(measure_counts_units_per_encoding) {
    std::string_view content = "a\xe4\xbd\xa0\xf0\x9f\x99\x82z";

    PositionMapper utf8_converter(content, PositionEncoding::UTF8);
    PositionMapper utf16_converter(content, PositionEncoding::UTF16);
    PositionMapper utf32_converter(content, PositionEncoding::UTF32);

    EXPECT_EQ(utf8_converter.measure(content), 9U);
    EXPECT_EQ(utf16_converter.measure(content), 5U);
    EXPECT_EQ(utf32_converter.measure(content), 4U);
}

TEST_CASE(character_and_length_follow_encoding_units) {
    std::string_view content = "a\xe4\xbd\xa0\xf0\x9f\x99\x82z\n";

    PositionMapper utf8_converter(content, PositionEncoding::UTF8);
    PositionMapper utf16_converter(content, PositionEncoding::UTF16);
    PositionMapper utf32_converter(content, PositionEncoding::UTF32);

    EXPECT_EQ(utf8_converter.character(0, 9), 9U);
    EXPECT_EQ(utf16_converter.character(0, 9), 5U);
    EXPECT_EQ(utf32_converter.character(0, 9), 4U);

    EXPECT_EQ(utf8_converter.length(0, 1, 8), 7U);
    EXPECT_EQ(utf16_converter.length(0, 1, 8), 3U);
    EXPECT_EQ(utf32_converter.length(0, 1, 8), 2U);

    EXPECT_EQ(utf8_converter.length(0, 8, 8), 0U);
    EXPECT_EQ(utf16_converter.length(0, 8, 8), 0U);
    EXPECT_EQ(utf32_converter.length(0, 8, 8), 0U);
}

TEST_CASE(round_trip_codepoint_boundaries_multiline) {
    std::string_view content = "a\xe4\xbd\xa0\n\xf0\x9f\x99\x82" "b";
    constexpr std::uint32_t boundaries[] = {0, 1, 4, 5, 9, 10};

    for(auto encoding: {PositionEncoding::UTF8, PositionEncoding::UTF16, PositionEncoding::UTF32}) {
        PositionMapper converter(content, encoding);
        for(auto offset: boundaries) {
            auto position = converter.to_position(offset);
            ASSERT_EQ(converter.to_offset(position), offset);
        }
    }
}

TEST_CASE(measure_tolerates_invalid_utf8_sequences) {
    const char raw[] = {'a', static_cast<char>(0xF0), static_cast<char>(0x9F), 'b'};
    std::string_view content(raw, sizeof(raw));

    PositionMapper utf8_converter(content, PositionEncoding::UTF8);
    PositionMapper utf16_converter(content, PositionEncoding::UTF16);
    PositionMapper utf32_converter(content, PositionEncoding::UTF32);

    EXPECT_EQ(utf8_converter.measure(content), 4U);
    EXPECT_EQ(utf16_converter.measure(content), 4U);
    EXPECT_EQ(utf32_converter.measure(content), 4U);
}

};  // TEST_SUITE(language_position)

}  // namespace eventide::language
