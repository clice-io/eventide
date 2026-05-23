/// Reproduction for simdjson string buffer overflow when decoding an untagged
/// variant<StructA, StructB> where the JSON contains a large string field.
///
/// The variant decoder uses try_read to attempt each alternative. When the
/// first alternative fails (e.g., missing required field), the large string
/// has already been unescaped into simdjson's internal string buffer. Because
/// the sub-reader wraps a Value (not a Document), try_read cannot rewind the
/// string buffer position. The second alternative then consumes additional
/// string buffer space, overflowing the allocation.

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"

namespace kota::codec {

namespace {

using json::from_json;

// ---- Mimic LSP TextDocumentContentChangeEvent types ----

struct Position {
    unsigned int line = 0;
    unsigned int character = 0;
};

struct Range {
    Position start;
    Position end;
};

// Partial change: has range + text (range is required).
struct ChangePartial {
    Range range;
    std::optional<unsigned int> range_length;
    std::string text;
};

// Whole-document change: just text.
struct ChangeWhole {
    std::string text;
};

using ChangeEvent = std::variant<ChangePartial, ChangeWhole>;

struct TextDocId {
    std::string uri;
    int version = 0;
};

struct DidChangeParams {
    TextDocId text_document;
    std::vector<ChangeEvent> content_changes;
};

// Build didChange JSON with a whole-document change of the given text size.
std::string make_whole_change(std::size_t text_size) {
    std::string text(text_size, 'A');
    std::string json;
    json.reserve(text_size + 256);
    json +=
        R"({"text_document":{"uri":"file:///test.cpp","version":1},"content_changes":[{"text":")";
    json += text;
    json += R"("}]})";
    return json;
}

// Build didChange JSON with a partial change of the given text size.
std::string make_partial_change(std::size_t text_size) {
    std::string text(text_size, 'B');
    std::string json;
    json.reserve(text_size + 512);
    json +=
        R"({"text_document":{"uri":"file:///test.cpp","version":2},"content_changes":[{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":5}},"text":")";
    json += text;
    json += R"("}]})";
    return json;
}

TEST_SUITE(serde_simdjson_variant_large_string) {

TEST_CASE(whole_doc_change_32KB) {
    auto json = make_whole_change(32768);
    DidChangeParams params;
    auto result = from_json(json, params);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(params.content_changes.size(), 1U);
    ASSERT_EQ(params.content_changes[0].index(), 1U);
    EXPECT_EQ(std::get<ChangeWhole>(params.content_changes[0]).text.size(), 32768U);
}

TEST_CASE(whole_doc_change_256KB) {
    auto json = make_whole_change(256 * 1024);
    DidChangeParams params;
    auto result = from_json(json, params);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(params.content_changes.size(), 1U);
    ASSERT_EQ(params.content_changes[0].index(), 1U);
    EXPECT_EQ(std::get<ChangeWhole>(params.content_changes[0]).text.size(), 256U * 1024U);
}

TEST_CASE(partial_change_32KB) {
    auto json = make_partial_change(32768);
    DidChangeParams params;
    auto result = from_json(json, params);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(params.content_changes.size(), 1U);
    ASSERT_EQ(params.content_changes[0].index(), 0U);
    EXPECT_EQ(std::get<ChangePartial>(params.content_changes[0]).text.size(), 32768U);
}

};  // TEST_SUITE(serde_simdjson_variant_large_string)

}  // namespace

}  // namespace kota::codec
