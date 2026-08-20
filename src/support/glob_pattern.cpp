#include "kota/support/glob_pattern.h"

#include <algorithm>
#include <format>
#include <limits>
#include <optional>
#include <ranges>

#include "kota/support/expected_try.h"

namespace kota {

namespace {

using GlobCharSet = std::bitset<256>;

std::expected<GlobCharSet, GlobError> parse_bracket_charset(std::string_view s) {
    GlobCharSet bv{};

    // Index of the last character consumed as a range end, so the dash in
    // `[a-c-e]` reads as a literal `-` instead of chaining a `c-e` range.
    std::uint32_t last_range_end = std::numeric_limits<std::uint32_t>::max();

    for(std::uint32_t i = 0, e = static_cast<std::uint32_t>(s.size()); i < e; ++i) {
        switch(s[i]) {
            case '\\': {
                auto backslash_pos = i;
                ++i;
                if(i == e) [[unlikely]] {
                    return std::unexpected{
                        GlobError{GlobError::StrayBackslash,
                                  backslash_pos, backslash_pos + 1,
                                  "stray `\\`"}
                    };
                }
                if(s[i] != '/') {
                    bv.set(static_cast<std::uint8_t>(s[i]), true);
                }
                break;
            }

            case '-': {
                if(i == 0 || i + 1 == e || i - 1 == last_range_end) {
                    bv.set('-', true);
                    break;
                }
                auto dash_pos = i;
                auto c_begin = static_cast<std::uint8_t>(s[i - 1]);
                auto c_end = static_cast<std::uint8_t>(s[i + 1]);
                ++i;
                if(c_end == '\\') {
                    auto backslash_pos = i;
                    ++i;
                    if(i == e) [[unlikely]] {
                        return std::unexpected{
                            GlobError{GlobError::StrayBackslash,
                                      backslash_pos, backslash_pos + 1,
                                      "stray `\\`"}
                        };
                    }
                    c_end = static_cast<std::uint8_t>(s[i]);
                }
                if(c_begin > c_end) [[unlikely]] {
                    return std::unexpected{
                        GlobError{GlobError::InvalidRange,
                                  dash_pos - 1,
                                  dash_pos + 2,
                                  std::format("`{}` is larger than `{}`", c_begin, c_end)}
                    };
                }
                for(std::uint32_t c = c_begin; c <= c_end; ++c) {
                    if(c != '/') {
                        bv.set(static_cast<std::uint8_t>(c), true);
                    }
                }
                last_range_end = i;
                break;
            }

            default: {
                if(s[i] != '/') {
                    bv.set(static_cast<std::uint8_t>(s[i]), true);
                }
            }
        }
    }

    return bv;
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
        auto bv = invert ? parse_bracket_charset(chars.substr(1)) : parse_bracket_charset(chars);
        if(!bv.has_value()) [[unlikely]] {
            return std::unexpected{std::move(bv.error())};
        }
        if(invert) {
            bv->flip();
            bv->set('/', false);
        }
        pat.brackets.push_back(Bracket{j + 1, std::move(*bv)});
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

    for(auto& glob: sub_globs) {
        if(glob.match(str)) {
            return true;
        }
    }
    return false;
}

/// Maximum number of backtrack iterations before aborting a match.
/// This provides protection against ReDoS (Regular expression Denial of Service)
/// attacks where crafted patterns and inputs could cause exponential backtracking.
constexpr static size_t max_backtrack_iterations = 65536;

bool GlobPattern::SubGlobPattern::match(std::string_view str) const {
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
                        if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
                            break;
                        }
                        ++p;
                        ++s;
                        continue;
                    }
                    break;
                }

                case '[': {
                    if(b < brackets.size() && brackets[b].bytes[std::uint8_t(*s)]) {
                        if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
                            break;
                        }
                        p = pat.data() + brackets[b].next_offset;
                        ++b;
                        ++s;
                        continue;
                    }
                    break;
                }

                case '\\': {
                    if(p + 1 != seg_end && *(p + 1) == *s) {
                        if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
                            break;
                        }
                        p += 2;
                        ++s;
                        continue;
                    }
                    break;
                }

                default: {
                    if(*p == *s) {
                        if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
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

        state.s += 1;
        s = state.s;
        p = state.p;
        b = state.b;
        current_glob_seg = state.glob_seg;
        seg_start = state.seg_start;
        seg_end = state.seg_end;
    }
}

}  // namespace kota
