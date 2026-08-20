#pragma once

#include <algorithm>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include "kota/support/small_vector.h"
#include "kota/support/string_ref.h"

namespace kota {

struct GlobError {
    enum Kind : std::uint8_t {
        UnmatchedBracket,
        StrayBackslash,
        NestedBrace,
        EmptyBrace,
        IncompleteBrace,
        InvalidRange,
        MultipleSlash,
        MultipleStar,
        TooManyExpansions,
        InvalidUtf8,
        InvalidEscape,
    };

    Kind kind;
    std::uint32_t begin;
    std::uint32_t end;
    std::string message;
};

/// Glob pattern matcher supporting VS Code-style glob syntax.
///
/// Supported syntax:
/// - `*` to match zero or more characters in a path segment
/// - `?` to match on one character in a path segment
/// - `**` to match any number of path segments, including none
/// - `{}` to group conditions (e.g. `**.{ts,js}`)
/// - `[]` to declare a range of characters (e.g., `example.[0-9]`)
/// - `[!...]` to negate a range (e.g., `example.[!0-9]`)
///
/// Note: Use only `/` for path segment separator. `/` cannot be escaped;
/// `\/` is rejected at create() with InvalidEscape.
///
/// A pattern whose body — or any brace-expanded arm of it — is exactly
/// `*` or `**` matches every path, including across `/` (see
/// is_trivial_match_all); anywhere else `*` stays within one segment.
///
/// Patterns must be valid UTF-8; create() rejects anything else with
/// InvalidUtf8. Matching is Unicode-aware: `?`, `[]` and escaped literals
/// consume one decoded code point at a time, while `*`, `**` and literal
/// runs stay byte-level (UTF-8 is self-synchronizing, so this cannot
/// change which code points they cover). Matched paths need not be valid
/// UTF-8: an input byte that does not decode counts as one character that
/// `?`, `*`, `**` and negated classes can cover, but no literal or
/// positive range can name it.
class GlobPattern {
public:
    [[nodiscard]] static std::expected<GlobPattern, GlobError>
        create(std::string_view s, size_t max_subpattern_num = 100);

    [[nodiscard]] bool is_trivial_match_all() const {
        if(!prefix.empty() || prefix_at_seg_end) {
            return false;
        }
        // Brace arms are independent OR alternatives, so one match-all arm
        // makes the whole pattern match-all: `{*,foo}` accepts no less
        // than `*`.
        return std::ranges::any_of(sub_globs, [](const SubGlobPattern& glob) {
            auto pat = glob.pattern();
            return pat == "*" || pat == "**";
        });
    }

    [[nodiscard]] bool match(std::string_view s) const;

private:
    std::string prefix;
    bool prefix_at_seg_end = false;

    struct SubGlobPattern {
        [[nodiscard]] static std::expected<SubGlobPattern, GlobError> create(std::string_view s);

        /// `start_at_seg_boundary` says whether `str` begins at a path
        /// segment boundary of the original input; it is false when a
        /// literal prefix was stripped mid-segment.
        [[nodiscard]] bool match(std::string_view str, bool start_at_seg_boundary) const;

        [[nodiscard]] std::string_view pattern() const {
            return std::string_view{pat.data(), pat.size()};
        }

        struct Bracket {
            size_t next_offset;
            bool negated = false;
            /// Inclusive code-point ranges, sorted and non-overlapping.
            small_vector<std::pair<char32_t, char32_t>, 2> ranges;

            [[nodiscard]] bool contains(char32_t cp) const;
        };

        small_vector<Bracket, 0> brackets;

        struct GlobSegment {
            size_t start;
            size_t end;
        };

        small_vector<GlobSegment, 6> glob_segments;
        small_vector<char, 0> pat;

    private:
        struct BacktrackState {
            size_t b;
            size_t glob_seg;
            bool wild_mode;
            const char* p;
            const char* s;
            const char* seg_end;
            const char* seg_start;
        };
    };

    small_vector<SubGlobPattern, 1> sub_globs;
};

}  // namespace kota
