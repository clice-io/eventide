#include "kota/support/glob_pattern.h"

#include <algorithm>
#include <format>
#include <limits>
#include <optional>
#include <ranges>

#include "kota/support/expected_try.h"

namespace kota {

namespace {

/// One matching unit: a decoded Unicode scalar value, or a byte that is
/// not valid UTF-8 mapped above the Unicode range so it only compares
/// equal to the same byte. Patterns are validated to be UTF-8 at create(),
/// so invalid atoms can only come from the matched path.
struct Utf8Atom {
    char32_t cp;
    std::uint32_t len;
};

constexpr char32_t invalid_atom_base = 0x110000;

Utf8Atom decode_utf8_atom(const char* it, const char* end) {
    const auto lead = static_cast<std::uint8_t>(*it);
    if(lead < 0x80) [[likely]] {
        return {lead, 1};
    }

    const auto invalid = Utf8Atom{invalid_atom_base + lead, 1};

    std::uint32_t len;
    char32_t cp;
    if((lead & 0xE0) == 0xC0) {
        len = 2;
        cp = lead & 0x1F;
    } else if((lead & 0xF0) == 0xE0) {
        len = 3;
        cp = lead & 0x0F;
    } else if((lead & 0xF8) == 0xF0) {
        len = 4;
        cp = lead & 0x07;
    } else {
        return invalid;
    }

    if(end - it < static_cast<std::ptrdiff_t>(len)) {
        return invalid;
    }
    for(std::uint32_t k = 1; k < len; ++k) {
        const auto cont = static_cast<std::uint8_t>(it[k]);
        if((cont & 0xC0) != 0x80) {
            return invalid;
        }
        cp = (cp << 6) | (cont & 0x3F);
    }

    // Reject overlong encodings, surrogates and out-of-range values so a
    // decoded atom never aliases a differently-spelled byte sequence.
    constexpr char32_t min_for_len[] = {0, 0, 0x80, 0x800, 0x10000};
    if(cp < min_for_len[len] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return invalid;
    }
    return {cp, len};
}

/// Byte offset of the first ill-formed UTF-8 subsequence, or nullopt.
std::optional<std::uint32_t> find_invalid_utf8(std::string_view s) {
    const char* it = s.data();
    const char* const end = it + s.size();
    while(it != end) {
        const auto atom = decode_utf8_atom(it, end);
        if(atom.cp >= invalid_atom_base) {
            return static_cast<std::uint32_t>(it - s.data());
        }
        it += atom.len;
    }
    return std::nullopt;
}

using CharClassRanges = small_vector<std::pair<char32_t, char32_t>, 2>;

std::expected<CharClassRanges, GlobError> parse_bracket_charset(std::string_view s) {
    CharClassRanges ranges;
    const char* it = s.data();
    const char* const end = it + s.size();
    auto offset = [&](const char* q) {
        return static_cast<std::uint32_t>(q - s.data());
    };

    // Decode one class member, resolving a leading `\` escape.
    auto next_member = [&]() -> std::expected<char32_t, GlobError> {
        if(*it == '\\') {
            auto backslash_pos = offset(it);
            ++it;
            if(it == end) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::StrayBackslash,
                              backslash_pos, backslash_pos + 1,
                              "stray `\\`"}
                };
            }
        }
        const auto atom = decode_utf8_atom(it, end);
        it += atom.len;
        return atom.cp;
    };

    // A member is held back one step so a following `-` can turn it into
    // the lower bound of a range. `-` first or last in the class stays a
    // literal member, and so does the `-` right after a completed range:
    // `[a-c-e]` reads as `a-c`, literal `-`, `e`.
    std::optional<char32_t> pending;
    std::uint32_t pending_begin = 0;

    while(it != end) {
        if(*it == '-' && pending.has_value() && it + 1 != end) {
            ++it;
            KOTA_EXPECTED_TRY_V(auto hi, next_member());
            if(*pending > hi) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::InvalidRange,
                              pending_begin, offset(it),
                              std::format("`U+{:04X}` is larger than `U+{:04X}`",
                              static_cast<std::uint32_t>(*pending),
                              static_cast<std::uint32_t>(hi))}
                };
            }
            ranges.push_back({*pending, hi});
            pending.reset();
            continue;
        }

        auto member_begin = offset(it);
        KOTA_EXPECTED_TRY_V(auto cp, next_member());
        if(pending.has_value()) {
            ranges.push_back({*pending, *pending});
        }
        pending = cp;
        pending_begin = member_begin;
    }
    if(pending.has_value()) {
        ranges.push_back({*pending, *pending});
    }

    // Sort and coalesce so lookup can binary-search and the per-test cost
    // is bounded by the distinct span count, not the written member count
    // (the backtrack cap counts retries, not range comparisons).
    std::ranges::sort(ranges);
    CharClassRanges merged;
    for(auto range: ranges) {
        if(!merged.empty() && range.first <= merged.back().second + 1) {
            merged.back().second = std::max(merged.back().second, range.second);
        } else {
            merged.push_back(range);
        }
    }

    return merged;
}

std::expected<small_vector<std::string, 1>, GlobError>
    glob_parse_brace_expansions(std::string_view s, size_t max_subpattern_num) {
    small_vector<std::string, 1> subpatterns;
    subpatterns.emplace_back(s);
    if(max_subpattern_num == 0 || !s.contains('{')) {
        return subpatterns;
    }

    struct BraceExpansion {
        std::uint32_t start;
        std::uint32_t length;
        small_vector<std::string_view, 2> terms;
    };

    small_vector<BraceExpansion, 0> brace_expansions;

    BraceExpansion* current_be = nullptr;
    std::uint32_t term_begin = 0;
    for(std::uint32_t i = 0, e = static_cast<std::uint32_t>(s.size()); i != e; ++i) {
        if(s[i] == '[') {
            auto bracket_pos = i;
            ++i;
            if(i == e) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::UnmatchedBracket,
                              bracket_pos, bracket_pos + 1,
                              "unmatched `[`"}
                };
            }
            if(s[i] == ']') {
                ++i;
            }
            while(i != e && s[i] != ']') {
                if(s[i] == '\\') {
                    auto backslash_pos = i;
                    ++i;
                    if(i == e) [[unlikely]] {
                        return std::unexpected{
                            GlobError{GlobError::StrayBackslash,
                                      backslash_pos, backslash_pos + 1,
                                      "unmatched `[` with stray `\\` inside"}
                        };
                    }
                }
                ++i;
            }
            if(i == e) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::UnmatchedBracket,
                              bracket_pos, bracket_pos + 1,
                              "unmatched `[`"}
                };
            }
        } else if(s[i] == '{') {
            if(current_be) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::NestedBrace,
                              i, i + 1,
                              "nested brace expansions are not supported"}
                };
            }
            current_be = &brace_expansions.emplace_back();
            current_be->start = i;
            term_begin = i + 1;
        } else if(s[i] == ',') {
            if(!current_be) {
                continue;
            }
            current_be->terms.push_back(s.substr(term_begin, i - term_begin));
            term_begin = i + 1;
        } else if(s[i] == '}') {
            if(!current_be) {
                continue;
            }
            if(current_be->terms.empty() && i - term_begin == 0) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::EmptyBrace,
                              current_be->start,
                              i + 1,
                              "empty brace expression"}
                };
            }
            current_be->terms.push_back(s.substr(term_begin, i - term_begin));
            current_be->length = i - current_be->start + 1;
            current_be = nullptr;
        } else if(s[i] == '\\') {
            auto backslash_pos = i;
            ++i;
            if(i == e) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::StrayBackslash,
                              backslash_pos, backslash_pos + 1,
                              "stray `\\`"}
                };
            }
        }
    }

    if(current_be) [[unlikely]] {
        return std::unexpected{
            GlobError{GlobError::IncompleteBrace,
                      current_be->start,
                      current_be->start + 1,
                      "incomplete brace expansion"}
        };
    }

    size_t subpattern_num = 1;
    for(auto& be: brace_expansions) {
        if(subpattern_num > std::numeric_limits<size_t>::max() / be.terms.size()) {
            subpattern_num = std::numeric_limits<size_t>::max();
            break;
        }
        subpattern_num *= be.terms.size();
    }

    if(subpattern_num > max_subpattern_num) [[unlikely]] {
        return std::unexpected{
            GlobError{GlobError::TooManyExpansions, 0, 0, "too many brace expansions"}
        };
    }

    for(auto& be: brace_expansions | std::views::reverse) {
        small_vector<std::string, 1> orig_sub_patterns;
        std::swap(subpatterns, orig_sub_patterns);
        for(std::string_view term: be.terms) {
            for(std::string_view orig: orig_sub_patterns) {
                subpatterns.emplace_back(orig).replace(be.start, be.length, term);
            }
        }
    }

    return subpatterns;
}

}  // namespace

std::expected<GlobPattern, GlobError> GlobPattern::create(std::string_view s,
                                                          size_t max_subpattern_num) {
    // Validate the whole pattern before any processing. Prefix extraction
    // and brace expansion only cut at ASCII bytes, so every derived
    // sub-pattern of a valid string is itself valid — matching can then
    // decode pattern bytes without ever seeing an ill-formed sequence.
    if(auto pos = find_invalid_utf8(s)) [[unlikely]] {
        return std::unexpected{
            GlobError{GlobError::InvalidUtf8, *pos, *pos + 1, "pattern is not valid UTF-8"}
        };
    }

    GlobPattern pat;
    size_t prefix_size = s.find_first_of("?*[{\\");
    auto check_consecutive_slashes = [](std::string_view str) -> std::optional<std::uint32_t> {
        bool prev_was_slash = false;
        for(std::uint32_t i = 0, e = static_cast<std::uint32_t>(str.size()); i < e; ++i) {
            if(str[i] == '/') {
                if(prev_was_slash) {
                    return i;
                }
                prev_was_slash = true;
            } else {
                prev_was_slash = false;
            }
        }
        return std::nullopt;
    };

    if(prefix_size == std::string_view::npos) {
        pat.prefix = std::string(s);
        if(auto pos = check_consecutive_slashes(pat.prefix)) [[unlikely]] {
            return std::unexpected{
                GlobError{GlobError::MultipleSlash,
                          *pos - 1,
                          *pos + 1,
                          "multiple `/` is not allowed"}
            };
        }
        return pat;
    }
    if(auto pos = check_consecutive_slashes(s.substr(0, prefix_size))) [[unlikely]] {
        return std::unexpected{
            GlobError{GlobError::MultipleSlash, *pos - 1, *pos + 1, "multiple `/` is not allowed"}
        };
    }
    if(prefix_size != 0 && s[prefix_size - 1] == '/') {
        pat.prefix_at_seg_end = true;
        --prefix_size;
    }
    pat.prefix = std::string(s.substr(0, prefix_size));
    s = s.substr(pat.prefix_at_seg_end ? prefix_size + 1 : prefix_size);

    KOTA_EXPECTED_TRY_V(auto sub_pats, glob_parse_brace_expansions(s, max_subpattern_num));

    for(auto& sub_pat: sub_pats) {
        KOTA_EXPECTED_TRY_V(auto res, SubGlobPattern::create(sub_pat));
        pat.sub_globs.push_back(std::move(res));
    }

    return pat;
}

std::expected<GlobPattern::SubGlobPattern, GlobError>
    GlobPattern::SubGlobPattern::create(std::string_view s) {
    SubGlobPattern pat;
    small_vector<GlobSegment, 6> glob_segments;
    GlobSegment* current_gs = &glob_segments.emplace_back();
    current_gs->start = 0;
    pat.pat.assign(s);

    std::uint32_t e = static_cast<std::uint32_t>(s.size());

    auto parse_bracket = [&](std::uint32_t i) -> std::expected<std::uint32_t, GlobError> {
        auto bracket_pos = i - 1;
        std::uint32_t j = i;
        if(j == e) [[unlikely]] {
            return std::unexpected{
                GlobError{GlobError::UnmatchedBracket,
                          bracket_pos, bracket_pos + 1,
                          "unmatched `[`"}
            };
        }
        if(s[j] == ']') {
            ++j;
        }
        while(j != e && s[j] != ']') {
            ++j;
            if(s[j - 1] == '\\') {
                if(j == e) [[unlikely]] {
                    return std::unexpected{
                        GlobError{GlobError::StrayBackslash,
                                  j - 1,
                                  j, "unmatched `[` with stray `\\` inside"}
                    };
                }
                ++j;
            }
        }
        if(j == e) [[unlikely]] {
            return std::unexpected{
                GlobError{GlobError::UnmatchedBracket,
                          bracket_pos, bracket_pos + 1,
                          "unmatched `[`"}
            };
        }

        std::string_view chars = s.substr(i, j - i);
        bool invert = s[i] == '^' || s[i] == '!';
        auto ranges = parse_bracket_charset(invert ? chars.substr(1) : chars);
        if(!ranges.has_value()) [[unlikely]] {
            // The class parser reports offsets relative to the class body;
            // rebase onto this sub-pattern (the negation prefix counts).
            auto base = i + (invert ? 1 : 0);
            ranges.error().begin += base;
            ranges.error().end += base;
            return std::unexpected{std::move(ranges.error())};
        }
        pat.brackets.push_back(Bracket{j + 1, invert, std::move(*ranges)});
        return j;
    };

    for(std::uint32_t i = 0; i < e; ++i) {
        if(!current_gs) {
            current_gs = &glob_segments.emplace_back();
            current_gs->start = i;
        }
        if(s[i] == '[') {
            auto result = parse_bracket(i + 1);
            if(!result.has_value()) [[unlikely]] {
                return std::unexpected{std::move(result.error())};
            }
            i = *result;
        } else if(s[i] == '\\') {
            auto backslash_pos = i;
            if(++i == e) [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::StrayBackslash,
                              backslash_pos, backslash_pos + 1,
                              "stray `\\`"}
                };
            }
            // An escaped `/` would slip past segment splitting and let
            // `\/\/` match `//` while a bare `//` is rejected; the
            // separator is structure, not a matchable character.
            if(s[i] == '/') [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::InvalidEscape,
                              backslash_pos, backslash_pos + 2,
                              "`/` cannot be escaped"}
                };
            }
        } else if(s[i] == '/') {
            if(i > 0 && s[i - 1] == '/') [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::MultipleSlash,
                              i - 1,
                              i + 1,
                              "multiple `/` is not allowed"}
                };
            }
            current_gs->end = i;
            current_gs = nullptr;
        } else if(s[i] == '*') {
            if(i + 2 < e && s[i + 1] == '*' && s[i + 2] == '*') [[unlikely]] {
                return std::unexpected{
                    GlobError{GlobError::MultipleStar, i, i + 3, "multiple `*` is not allowed"}
                };
            }
        }
    }

    if(current_gs) {
        current_gs->end = e;
    }

    pat.glob_segments.assign(std::move(glob_segments));
    return pat;
}

bool GlobPattern::SubGlobPattern::Bracket::contains(char32_t cp) const {
    // A bracket never matches the segment separator, negated or not.
    if(cp == U'/') {
        return false;
    }
    // Ranges are sorted and disjoint: the candidate range is the first one
    // whose upper bound reaches cp.
    auto it = std::ranges::lower_bound(ranges, cp, {}, &std::pair<char32_t, char32_t>::second);
    const bool hit = it != ranges.end() && it->first <= cp;
    return negated ? !hit : hit;
}

/// Whether the pattern tail matches the empty input: every star run matches
/// nothing and every `/` is absorbed by an adjacent globstar, each globstar
/// absorbing at most one separator: `x*` and `x*/**` accept a bare `x`,
/// `x*/*` and `x*/**/*` do not. With `leading_separator`, a `/` already
/// stripped before `tail` must be absorbed by its leading globstar as well.
static bool glob_tail_matches_empty(std::string_view tail, bool leading_separator) {
    const char* q = tail.data();
    const char* const q_end = q + tail.size();
    size_t run = 0;
    while(q != q_end && *q == '*') {
        q += 1;
        run += 1;
    }
    bool used = false;
    if(leading_separator) {
        if(run < 2) {
            return false;
        }
        used = true;
    }
    while(q != q_end) {
        if(*q != '/') {
            return false;
        }
        q += 1;
        size_t next = 0;
        while(q != q_end && *q == '*') {
            q += 1;
            next += 1;
        }
        if(run >= 2 && !used) {
            used = false;
        } else if(next >= 2) {
            used = true;
        } else {
            return false;
        }
        run = next;
    }
    return true;
}

bool GlobPattern::match(std::string_view sv) const {
    if(is_trivial_match_all()) {
        return true;
    }

    string_ref str(sv);
    if(!str.consume_front(prefix)) {
        return false;
    }

    if(str.empty() && sub_globs.empty()) {
        return true;
    }

    if(prefix_at_seg_end) {
        if(str.empty()) {
            // The `/` after the prefix went unmatched; a sub glob absorbs it
            // only when its whole pattern matches empty behind a separator,
            // exactly as if the prefix had never been split off.
            return std::ranges::any_of(sub_globs, [](const SubGlobPattern& glob) {
                return glob_tail_matches_empty(glob.pattern(), true);
            });
        }
        if(str[0] != '/') {
            return false;
        }
        str = str.substr(1);
    }

    bool at_seg_boundary = prefix.empty() || prefix_at_seg_end;
    for(auto& glob: sub_globs) {
        if(glob.match(str, at_seg_boundary)) {
            return true;
        }
    }
    return false;
}

/// Maximum number of backtrack iterations before aborting a match.
/// This provides protection against ReDoS (Regular expression Denial of Service)
/// attacks where crafted patterns and inputs could cause exponential backtracking.
constexpr static size_t max_backtrack_iterations = 65536;

bool GlobPattern::SubGlobPattern::match(std::string_view str, bool start_at_seg_boundary) const {
    const char* s = str.data();
    const char* const s_start = s;
    const char* const s_end = s + str.size();
    const char* p = pat.data();
    const char* seg_start = p;
    const char* const p_start = p;
    const char* const p_end = p + pat.size();
    const char* seg_end = p + glob_segments[0].end;
    size_t b = 0;
    size_t current_glob_seg = 0;

    small_vector<BacktrackState, 6> backtrack_stack;
    const size_t seg_num = glob_segments.size();
    size_t backtrack_iterations = 0;

    auto segment_range = [&](size_t idx) -> std::pair<const char*, const char*> {
        return {p_start + glob_segments[idx].start, p_start + glob_segments[idx].end};
    };

    auto push_backtrack =
        [&backtrack_stack, &b, &current_glob_seg, &p, &s, &seg_end, &seg_start](bool wild) {
            backtrack_stack.push_back({.b = b,
                                       .glob_seg = current_glob_seg,
                                       .wild_mode = wild,
                                       .p = p,
                                       .s = s,
                                       .seg_end = seg_end,
                                       .seg_start = seg_start});
        };

    // A pattern segment must begin where an input segment begins. `s_start`
    // qualifies only when the prefix was stripped at a boundary; otherwise
    // segment 0 alone may continue the prefix's segment there (`a?` vs
    // `ab`), while a globstar handing over to a later segment may not
    // (`a**/?` vs `aa`).
    auto at_input_seg_start = [&] {
        if(s == s_start) {
            return start_at_seg_boundary || current_glob_seg == 0;
        }
        return *(s - 1) == '/';
    };

    while(true) {
        if(s == s_end) {
            return glob_tail_matches_empty({p, p_end}, false);
        }

        // Handle segment boundary first (early-continue)
        if(p == seg_end && seg_end != p_end) {
            // The pattern segment is complete, so the input segment must end
            // here too; on mismatch fall through to backtracking so an
            // earlier star can retry (`*a/b` vs `aa/b`).
            if(*s == '/') {
                if(current_glob_seg + 1 < seg_num) {
                    s += 1;
                    current_glob_seg += 1;
                    auto [new_start, new_end] = segment_range(current_glob_seg);
                    p = new_start;
                    seg_start = new_start;
                    seg_end = new_end;
                    continue;
                }
                // Trailing `/` in the pattern: done only if the input ends
                // here too; otherwise fall through untouched so backtracking
                // can realign an earlier `**` (`**/a/` vs `a/a/`).
                if(s + 1 == s_end) {
                    return true;
                }
            }
        }

        if(p != seg_end) {
            switch(*p) {
                case '*': {
                    // Handle single `*` first (simpler case)
                    if(p + 1 == p_end || *(p + 1) != '*') {
                        ++p;
                        if(p == seg_end) {
                            // The star is the segment's tail: consuming
                            // exactly up to the next `/` (or the end of the
                            // input) is the only possible span, so no
                            // backtrack state is needed.
                            while(s != s_end && *s != '/') {
                                ++s;
                            }
                        } else {
                            push_backtrack(false);
                        }
                        continue;
                    }
                    // Handle `**` case
                    p += 2;
                    // Consume additional stars within this segment only
                    while(p != seg_end && *p == '*') {
                        ++p;
                    }
                    if(p == seg_end) {
                        if(current_glob_seg + 1 == seg_num) {
                            return true;
                        }
                        ++current_glob_seg;
                        auto [new_start, new_end] = segment_range(current_glob_seg);
                        p = new_start;
                        seg_start = new_start;
                        seg_end = new_end;
                    }
                    push_backtrack(true);
                    continue;
                }

                case '?': {
                    if(s != s_end && *s != '/') {
                        if(p == seg_start && !at_input_seg_start()) {
                            break;
                        }
                        ++p;
                        s += decode_utf8_atom(s, s_end).len;
                        continue;
                    }
                    break;
                }

                case '[': {
                    if(b < brackets.size()) {
                        const auto atom = decode_utf8_atom(s, s_end);
                        if(brackets[b].contains(atom.cp)) {
                            if(p == seg_start && !at_input_seg_start()) {
                                break;
                            }
                            p = pat.data() + brackets[b].next_offset;
                            ++b;
                            s += atom.len;
                            continue;
                        }
                    }
                    break;
                }

                case '\\': {
                    if(p + 1 != seg_end) {
                        const auto escaped = decode_utf8_atom(p + 1, seg_end);
                        const auto atom = decode_utf8_atom(s, s_end);
                        if(escaped.cp == atom.cp) {
                            if(p == seg_start && !at_input_seg_start()) {
                                break;
                            }
                            p += 1 + escaped.len;
                            s += atom.len;
                            continue;
                        }
                    }
                    break;
                }

                default: {
                    if(*p == *s) {
                        if(p == seg_start && !at_input_seg_start()) {
                            break;
                        }
                        ++p;
                        ++s;
                        continue;
                    }
                    break;
                }
            }
        }
        // p == seg_end == p_end, or switch fell through: backtrack

        if(backtrack_stack.empty()) [[unlikely]] {
            return false;
        }

        if(++backtrack_iterations > max_backtrack_iterations) [[unlikely]] {
            return false;
        }

        auto& state = backtrack_stack.back();

        // Each retry extends the star's match by the character at state.s.
        // A single `*` may not consume `/`, so once that character is a
        // slash (or the input is exhausted) the state is dead; only `**`
        // (wild) extends across segment boundaries.
        if(state.s >= s_end || (!state.wild_mode && *state.s == '/')) {
            backtrack_stack.pop_back();
            continue;
        }

        // Whole atoms, not bytes: a star stopping inside a multi-byte
        // character would let the following `?`/`[]` decode its
        // continuation bytes as extra characters (`*?[!X]X` vs `中X`).
        state.s += decode_utf8_atom(state.s, s_end).len;
        s = state.s;
        p = state.p;
        b = state.b;
        current_glob_seg = state.glob_seg;
        seg_start = state.seg_start;
        seg_end = state.seg_end;
    }
}

}  // namespace kota
