/// Minimal reproduction for simdjson string buffer overflow when decoding
/// an untagged variant containing a large string field.
///
/// Bug: When kota::codec::json::from_json decodes an untagged
///   variant<StructA, StructB> from a JSON object, the variant decoder
///   uses try_read to attempt each alternative. For a sub-reader wrapping
///   a simdjson Value (not Document), try_read cannot rewind the string
///   buffer on failure. If the first alternative fails after unescaping a
///   large string, the string buffer position is not reclaimed. The second
///   alternative then writes past the buffer, causing a heap-buffer-overflow.
///
/// Build (from clice repo root, Debug config with ASan):
///   clang++ -std=c++23 -g -O0 -fsanitize=address \
///     -I build/Debug/_deps/kotatsu-src/include \
///     -I build/Debug/_deps/simdjson-src/include \
///     -DSIMDJSON_EXCEPTIONS=0 -DSIMDJSON_THREADS_ENABLED=1 \
///     -fno-exceptions -fno-rtti \
///     build/Debug/_deps/kotatsu-src/tests/repro_string_overflow.cpp \
///     build/Debug/lib/libsimdjson.a -lpthread \
///     -o build/Debug/repro_string_overflow

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "kota/codec/json/json.h"

// Mimic LSP TextDocumentContentChangeEvent types.
struct Position {
    unsigned int line = 0;
    unsigned int character = 0;
};

struct Range {
    Position start;
    Position end;
};

// Partial change: has range (required) + text.
struct ChangePartial {
    Range range;
    std::optional<unsigned int> range_length;
    std::string text;
};

// Whole-document change: just text.
struct ChangeWhole {
    std::string text;
};

// Untagged variant -- this triggers the bug.
using ChangeEvent = std::variant<ChangePartial, ChangeWhole>;

struct TextDocId {
    std::string uri;
    int version = 0;
};

struct DidChangeParams {
    TextDocId text_document;
    std::vector<ChangeEvent> content_changes;
};

int main() {
    // Whole-document change with 32KB text.
    // The variant decoder tries ChangePartial first (requires "range"),
    // fails, but the large "text" has already been unescaped into the
    // string buffer. Then it tries ChangeWhole, overflowing the buffer.
    std::string text(32768, 'A');
    std::string json;
    json.reserve(text.size() + 256);
    json += R"({"text_document":{"uri":"file:///test.cpp","version":1},"content_changes":[{"text":)";
    json += '"';
    json += text;
    json += R"("}]})";

    DidChangeParams params;
    auto result = kota::codec::json::from_json(json, params);

    if (!result.has_value()) {
        fprintf(stderr, "FAIL: parse error: %s\n", result.error().to_string().c_str());
        return 1;
    }

    if (params.content_changes.size() != 1 ||
        !std::holds_alternative<ChangeWhole>(params.content_changes[0])) {
        fprintf(stderr, "FAIL: unexpected variant alternative\n");
        return 1;
    }

    fprintf(stderr, "PASS (no overflow)\n");
    return 0;
}
