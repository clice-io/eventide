#include "kota/support/glob_pattern.h"

#include <format>
#include <limits>
#include <optional>
#include <ranges>

#include "kota/support/expected_try.h"

namespace kota {

namespace {

/// One matching unit of the subject or pattern text: a decoded Unicode
/// scalar value, or a byte that is not valid UTF-8 mapped above the
/// Unicode range so it only compares equal to the same byte.
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
    // the lower bound of a range; `-` first or last in the class stays a
    // literal member, as before.
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

    return ranges;
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
        auto ranges = parse_bracket_charset(invert ? chars.substr(1) : chars);
        if(!ranges.has_value()) [[unlikely]] {
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
    // A bracket never matches the segment separator, whether negated or not.
    if(cp == U'/') {
        return false;
    }
    const bool hit = std::ranges::any_of(ranges, [&](const auto& range) {
        return range.first <= cp && cp <= range.second;
    });
    return negated ? !hit : hit;
}

bool GlobPattern::match(std::string_view sv) const {
    string_ref str(sv);
    if(!str.consume_front(prefix)) {
        return false;
    }

    if(str.empty() && sub_globs.empty()) {
        return true;
    }

    if(!str.empty() && prefix_at_seg_end) {
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
    bool wild_mode = false;

    small_vector<BacktrackState, 6> backtrack_stack;
    const size_t seg_num = glob_segments.size();
    size_t backtrack_iterations = 0;

    auto segment_range = [&](size_t idx) -> std::pair<const char*, const char*> {
        return {p_start + glob_segments[idx].start, p_start + glob_segments[idx].end};
    };

    auto push_backtrack =
        [&backtrack_stack, &b, &current_glob_seg, &wild_mode, &p, &s, &seg_end, &seg_start]() {
            backtrack_stack.push_back({.b = b,
                                       .glob_seg = current_glob_seg,
                                       .wild_mode = wild_mode,
                                       .p = p,
                                       .s = s,
                                       .seg_end = seg_end,
                                       .seg_start = seg_start});
        };

    while(current_glob_seg < seg_num) {
        if(s == s_end) {
            return pattern().find_first_not_of("*/", p - pat.data()) == std::string_view::npos;
        }

        // Handle segment boundary first (early-continue)
        if(p == seg_end && seg_end != p_end) {
            if(wild_mode) {
                ++current_glob_seg;
                while(s != s_end && *s != '/') {
                    ++s;
                }
                if(s != s_end && *s == '/') {
                    ++s;
                }
                if(current_glob_seg >= seg_num) [[unlikely]] {
                    return s == s_end;
                }
                auto [new_start, new_end] = segment_range(current_glob_seg);
                p = new_start;
                seg_start = new_start;
                seg_end = new_end;
                continue;
            }
            // non-wild segment transition
            if(*seg_end != *s) {
                return false;
            }
            while(s != s_end && *s == '/') {
                ++s;
            }
            ++current_glob_seg;
            if(current_glob_seg >= seg_num) [[unlikely]] {
                break;
            }
            auto [new_start, new_end] = segment_range(current_glob_seg);
            p = new_start;
            seg_start = new_start;
            seg_end = new_end;
            continue;
        }

        if(p != seg_end) {
            switch(*p) {
                case '*': {
                    // Handle single `*` first (simpler case)
                    if(p + 1 == p_end || *(p + 1) != '*') {
                        ++p;
                        wild_mode = false;
                        if(p == seg_end) {
                            while(s != s_end && *s != '/') {
                                ++s;
                            }
                            if(s == s_end) {
                                continue;
                            }
                            if(s + 1 != s_end) {
                                ++s;
                            }
                            if(current_glob_seg + 1 == seg_num) {
                                return true;
                            }
                            ++current_glob_seg;
                            auto [new_start, new_end] = segment_range(current_glob_seg);
                            p = new_start;
                            seg_start = new_start;
                            seg_end = new_end;
                        }
                        push_backtrack();
                        continue;
                    }
                    // Handle `**` case
                    p += 2;
                    wild_mode = true;
                    // Consume additional stars within this segment only
                    while(p != seg_end && *p == '*') {
                        ++p;
                    }
                    if(p == seg_end) {
                        if(current_glob_seg + 1 == seg_num) {
                            return true;
                        }
                        ++current_glob_seg;
                        while(s != s_end && *s == '/') {
                            ++s;
                        }
                        auto [new_start, new_end] = segment_range(current_glob_seg);
                        p = new_start;
                        seg_start = new_start;
                        seg_end = new_end;
                    }
                    push_backtrack();
                    continue;
                }

                case '?': {
                    if(s != s_end && *s != '/') {
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
                            if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
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
                            if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
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

        if(state.s >= s_end) [[unlikely]] {
            backtrack_stack.pop_back();
            continue;
        }
        s = ++state.s;
        p = state.p;
        b = state.b;
        current_glob_seg = state.glob_seg;
        wild_mode = state.wild_mode;
        seg_start = state.seg_start;
        seg_end = state.seg_end;

        if(s > s_end) [[unlikely]] {
            backtrack_stack.pop_back();
            continue;
        }

        if(!wild_mode && (s == s_end || *s == '/')) {
            backtrack_stack.pop_back();
            continue;
        }
    }

    return s == s_end;
}

}  // namespace kota
