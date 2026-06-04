#include "kota/deco/option/table.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <expected>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace kota::option;

namespace {

enum class AcceptResult {
    Matched,
    NoMatch,
    MissingValue,
};

class ArgSpan {
    const void* data;
    std::uint32_t count;
    std::string_view (*access)(const void*, std::uint32_t);

public:
    ArgSpan(const void* data,
            std::uint32_t size,
            std::string_view (*access)(const void*, std::uint32_t)) :
        data(data), count(size), access(access) {}

    std::string_view operator[](std::uint32_t i) const {
        return access(data, i);
    }

    std::uint32_t size() const {
        return count;
    }
};

std::string_view ltrim_all_of(std::string_view str, const std::vector<char>& prefixes) {
    auto pos = str.find_first_not_of(prefixes.data(), 0, prefixes.size());
    if(pos != std::string_view::npos)
        return str.substr(pos, str.size());
    return "";
}

char safe_tolower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool starts_with_insensitive(std::string_view text, std::string_view prefix) {
    if(prefix.size() > text.size())
        return false;
    for(size_t i = 0; i < prefix.size(); ++i) {
        if(safe_tolower(text[i]) != safe_tolower(prefix[i]))
            return false;
    }
    return true;
}

int compare_insensitive(std::string_view str1, std::string_view str2) {
    size_t mn = std::min(str1.size(), str2.size());
    for(size_t i = 0; i < mn; ++i) {
        auto c1 = safe_tolower(str1[i]);
        auto c2 = safe_tolower(str2[i]);
        if(c1 == c2)
            continue;
        return (c1 < c2) ? -1 : 1;
    }
    if(str1.size() == str2.size())
        return 0;
    return str1.size() < str2.size() ? -1 : 1;
}

int str_cmp_opt_name(std::string_view a, std::string_view b, bool fallback_case_sensitive) {
    size_t min_sz = std::min(a.size(), b.size());
    if(int res = compare_insensitive(a.substr(0, min_sz), b.substr(0, min_sz)))
        return res;
    if(a.size() == b.size())
        return fallback_case_sensitive ? a.compare(b) : 0;
    return (a.size() == min_sz) ? 1 : -1;
}

struct OptNameLess {
    inline bool operator()(const Option& i, std::string_view name) const {
        return str_cmp_opt_name(i.name(), name, false) < 0;
    }
};

bool is_input(const OptTable* table, std::string_view arg) {
    if(arg == "-")
        return true;
    for(const auto& prefix: table->prefixes_union) {
        if(arg.starts_with(prefix))
            return false;
    }
    return true;
}

std::uint32_t match_opt(const Option* i, std::string_view str, bool ignore_case) {
    auto name = i->name();
    for(auto prefix: i->prefixes) {
        if(str.starts_with(prefix)) {
            auto rest = str.substr(prefix.size());
            bool matched =
                ignore_case ? starts_with_insensitive(rest, name) : rest.starts_with(name);
            if(matched)
                return static_cast<std::uint32_t>(prefix.size() + name.size());
        }
    }
    return 0;
}

void consume_unknown_values(const OptTable* table,
                            const Option* search_begin,
                            const Option* search_end,
                            bool ignore_case,
                            ArgSpan args,
                            std::uint32_t& index,
                            ParsedArg& out,
                            const ParseOptions& options) {
    while(index < args.size()) {
        auto next = args[index];
        if(next.empty() || next == "--")
            break;

        if(!is_input(table, next)) {
            bool found_known = false;
            for(auto* s = search_begin; s != search_end; ++s) {
                if(match_opt(s, next, ignore_case)) {
                    OptionRef opt(*s, *table);
                    if(!options.excludes(opt)) {
                        found_known = true;
                        break;
                    }
                }
            }
            if(found_known)
                break;
        }

        out.add_value(next);
        ++index;
    }
}

AcceptResult accept_internal(const OptionRef& opt,
                             ArgSpan args,
                             std::string_view spelling,
                             std::uint32_t& index,
                             ParsedArg& out) {
    const size_t spelling_sz = spelling.size();
    const size_t args_idx_sz = args[index].size();

    out.clear();
    out.spelling = spelling;

    switch(opt.kind()) {
        case Kind::Flag: {
            if(spelling_sz != args_idx_sz)
                return AcceptResult::NoMatch;
            out.index = index++;
            return AcceptResult::Matched;
        }
        case Kind::Joined: {
            auto value = args[index].substr(spelling_sz);
            out.add_value(value);
            out.index = index++;
            return AcceptResult::Matched;
        }
        case Kind::CommaJoined: {
            out.index = index;
            auto rest = args[index].substr(spelling_sz);
            for(const auto& part: std::views::split(rest, ',') |
                                      std::views::filter([](auto&& r) { return !r.empty(); })) {
                out.add_value(std::string_view(part));
            }
            index++;
            return AcceptResult::Matched;
        }
        case Kind::Separate: {
            if(spelling_sz != args_idx_sz)
                return AcceptResult::NoMatch;

            out.index = index;
            index += 2;
            if(index > args.size() || args[index - 1].empty())
                return AcceptResult::MissingValue;

            out.add_value(args[index - 1]);
            return AcceptResult::Matched;
        }
        case Kind::MultiArg: {
            if(spelling_sz != args_idx_sz)
                return AcceptResult::NoMatch;

            out.index = index;
            index += 1 + opt.num_args();
            if(index > args.size())
                return AcceptResult::MissingValue;

            for(std::uint32_t i = 0; i != opt.num_args(); ++i)
                out.add_value(args[index - opt.num_args() + i]);
            return AcceptResult::Matched;
        }
        case Kind::JoinedOrSeparate: {
            if(spelling_sz != args_idx_sz) {
                auto value = args[index].substr(spelling_sz);
                out.add_value(value);
                out.index = index++;
                return AcceptResult::Matched;
            }

            out.index = index;
            index += 2;
            if(index > args.size() || args[index - 1].empty())
                return AcceptResult::MissingValue;

            out.add_value(args[index - 1]);
            return AcceptResult::Matched;
        }
        case Kind::JoinedAndSeparate: {
            out.index = index;
            index += 2;
            if(index > args.size() || args[index - 1].empty())
                return AcceptResult::MissingValue;

            out.add_value(args[index - 2].substr(spelling_sz));
            out.add_value(args[index - 1]);
            return AcceptResult::Matched;
        }
        case Kind::RemainingArgs: {
            if(spelling_sz != args_idx_sz)
                return AcceptResult::NoMatch;
            out.index = index++;
            while(index < args.size() && !args[index].empty())
                out.add_value(args[index++]);
            return AcceptResult::Matched;
        }
        case Kind::RemainingArgsJoined: {
            out.index = index;
            if(spelling_sz != args_idx_sz) {
                out.add_value(args[index].substr(spelling_sz));
            }
            index++;
            while(index < args.size() && !args[index].empty())
                out.add_value(args[index++]);
            return AcceptResult::Matched;
        }

        default: std::unreachable();
    }
}

AcceptResult accept_opt(const OptionRef& opt,
                        ArgSpan args,
                        std::string_view spelling,
                        bool grouped_short_option,
                        std::uint32_t& index,
                        ParsedArg& out) {
    AcceptResult result;

    if(grouped_short_option && opt.kind() == Kind::Flag) {
        out.clear();
        out.spelling = spelling;
        out.index = index;
        result = AcceptResult::Matched;
    } else {
        result = accept_internal(opt, args, spelling, index, out);
    }

    if(result != AcceptResult::Matched)
        return result;

    OptionRef unaliased_opt = opt.unaliased_option();
    if(opt.id() == unaliased_opt.id()) {
        out.id = opt.id();
        return AcceptResult::Matched;
    }

    out.id = unaliased_opt.id();

    if(opt.kind() != Kind::Flag)
        return AcceptResult::Matched;

    if(const char* val = opt.alias_args()) {
        while(*val != '\0') {
            out.add_value(std::string_view(val));
            val += std::strlen(val) + 1;
        }
    }
    if(unaliased_opt.kind() == Kind::Joined && !opt.alias_args()) {
        out.add_value(std::string_view(""));
    }
    return AcceptResult::Matched;
}

}  // namespace

OptTable::OptTable(std::span<const Option> option_infos,
                   bool ignore_case,
                   std::vector<std::string_view> prefixes_union) :
    option_infos(option_infos), prefixes_union(std::move(prefixes_union)),
    ignore_case(ignore_case) {
    bool found_searchable = false;
    for(std::uint32_t i = 0, e = static_cast<std::uint32_t>(option_infos.size()); i != e; ++i) {
        auto& info = option_infos[i];
        if(info.kind == Kind::Input) {
            assert(!this->input_option_id && "Cannot have multiple input options!");
            this->input_option_id = info.id;
        } else if(info.kind == Kind::Unknown) {
            assert(!this->unknown_option_id && "Cannot have multiple unknown options!");
            this->unknown_option_id = info.id;
        } else if(info.kind != Kind::Group && !found_searchable) {
            this->first_searchable_index = i;
            found_searchable = true;
        }
    }

    assert(this->unknown_option_id && "OptTable requires an Unknown option.");

    if(this->prefixes_union.empty()) {
        std::set<std::string_view> tmp;
        for(const auto& i: option_infos.subspan(this->first_searchable_index)) {
            for(auto prefix: i.prefixes)
                tmp.insert(prefix);
        }
        this->prefixes_union = std::vector<std::string_view>(tmp.begin(), tmp.end());
    }

    std::set<char> seen_chars;
    for(auto& prefix: this->prefixes_union) {
        seen_chars.insert(prefix.begin(), prefix.end());
    }
    this->prefix_chars.assign(seen_chars.begin(), seen_chars.end());
}

std::optional<OptionRef> OptTable::option(std::uint32_t opt_id) const {
    if(opt_id == 0)
        return std::nullopt;
    assert((opt_id - 1) < static_cast<std::uint32_t>(this->option_infos.size()) && "Invalid ID.");
    return OptionRef(this->option_infos[opt_id - 1], *this);
}

std::uint32_t OptTable::find_option(std::string_view argument, std::uint32_t vis) const {
    std::string arg_str(argument);
    if(argument.ends_with("="))
        arg_str += "placeholder";

    std::vector<std::string> args = {arg_str, "placeholder"};
    auto access = [](const void* d, std::uint32_t i) -> std::string_view {
        return static_cast<const std::string*>(d)[i];
    };
    std::uint32_t index = 0;
    ParsedArg out;

    ParseOptions opts;
    opts.visibility = vis;
    auto result = parse_step(*this,
                             args.data(),
                             static_cast<std::uint32_t>(args.size()),
                             access,
                             index,
                             out,
                             opts);
    if(result == static_cast<int>(AcceptResult::Matched))
        return out.id;
    return 0;
}

bool ParseOptions::excludes(const OptionRef& opt) const {
    if(!opt.has_visibility_flag(visibility))
        return true;
    if(include_flags && !opt.has_flag(include_flags))
        return true;
    if(exclude_flags && opt.has_flag(exclude_flags))
        return true;
    return false;
}

int kota::option::parse_step(const OptTable& table,
                             const void* data,
                             std::uint32_t size,
                             ArgAccessFn access_fn,
                             std::uint32_t& index,
                             ParsedArg& out,
                             const ParseOptions& options) {
    ArgSpan args(data, size, access_fn);
    std::uint32_t prev = index;
    auto str = args[index];

    if(is_input(&table, str)) {
        out.clear();
        out.id = table.input_option_id;
        out.spelling = str;
        out.index = index++;
        return static_cast<int>(AcceptResult::Matched);
    }

    auto search_start = options.input_random_index ? 0U : table.first_searchable_index;
    const auto* start = table.option_infos.data() + search_start;
    const auto* end = table.option_infos.data() + table.option_infos.size();
    auto name = ltrim_all_of(str, table.prefix_chars);

    start = (options.tablegen_mode) ? std::lower_bound(start, end, name, OptNameLess()) : start;

    if(options.tablegen_mode) {
        for(; start != end; ++start) {
            std::uint32_t arg_sz = 0;
            for(; start != end; ++start) {
                if(arg_sz = match_opt(start, str, table.ignore_case); arg_sz)
                    break;
            }
            if(start == end)
                break;

            OptionRef opt(*start, table);

            if(options.excludes(opt))
                continue;

            auto a = accept_opt(opt, args, args[prev].substr(0, arg_sz), false, index, out);
            if(a == AcceptResult::Matched)
                return static_cast<int>(AcceptResult::Matched);

            if(prev != index) {
                out.index = prev;
                return static_cast<int>(AcceptResult::MissingValue);
            }
        }
    } else {
        ParsedArg best_out;
        std::uint32_t best_match_size = 0;
        std::uint32_t best_index = prev;
        std::uint32_t missing_index = prev;
        std::uint32_t missing_match_size = 0;

        for(; start != end; ++start) {
            std::uint32_t arg_sz = 0;
            for(; start != end; ++start) {
                if(arg_sz = match_opt(start, str, table.ignore_case); arg_sz)
                    break;
            }
            if(start == end)
                break;

            OptionRef opt(*start, table);

            if(options.excludes(opt))
                continue;

            std::uint32_t candidate_index = prev;
            ParsedArg candidate_out;
            auto a = accept_opt(opt,
                                args,
                                args[prev].substr(0, arg_sz),
                                false,
                                candidate_index,
                                candidate_out);
            if(a == AcceptResult::Matched) {
                if(best_match_size < arg_sz) {
                    best_out = candidate_out;
                    best_match_size = arg_sz;
                    best_index = candidate_index;
                }
                continue;
            }

            if(candidate_index != prev && missing_match_size < arg_sz) {
                missing_index = candidate_index;
                missing_match_size = arg_sz;
            }
        }

        if(best_match_size > 0) {
            index = best_index;
            out = best_out;
            return static_cast<int>(AcceptResult::Matched);
        }

        if(missing_match_size != 0) {
            index = missing_index;
            out.index = prev;
            return static_cast<int>(AcceptResult::MissingValue);
        }
    }

    if(str[0] == '/') {
        out.clear();
        out.id = table.input_option_id;
        out.spelling = str;
        out.index = index++;
        return static_cast<int>(AcceptResult::Matched);
    }

    out.clear();
    out.id = table.unknown_option_id;
    out.spelling = str;
    out.index = index++;

    if(options.greedy_unknown && str != "--") {
        consume_unknown_values(&table,
                               table.option_infos.data() + search_start,
                               table.option_infos.data() + table.option_infos.size(),
                               table.ignore_case,
                               args,
                               index,
                               out,
                               options);
    }

    return static_cast<int>(AcceptResult::Matched);
}

int kota::option::parse_step_grouped(const OptTable& table,
                                     const void* data,
                                     std::uint32_t size,
                                     ArgAccessFn access_fn,
                                     std::uint32_t& index,
                                     ParsedArg& out,
                                     std::string& group_buf,
                                     const ParseOptions& options) {
    ArgSpan args(data, size, access_fn);
    auto str = args[index];

    if(is_input(&table, str)) {
        out.clear();
        out.id = table.input_option_id;
        out.spelling = str;
        out.index = index++;
        group_buf.clear();
        return static_cast<int>(AcceptResult::Matched);
    }

    auto search_start = options.input_random_index ? 0U : table.first_searchable_index;
    const auto* end_ptr = table.option_infos.data() + table.option_infos.size();
    auto name = ltrim_all_of(str, table.prefix_chars);
    const auto* start_ptr = (options.tablegen_mode)
                                ? std::lower_bound(table.option_infos.data() + search_start,
                                                   end_ptr,
                                                   name,
                                                   OptNameLess())
                                : table.option_infos.data() + search_start;

    const Option* fallback_opt = nullptr;
    std::uint32_t prev = index;

    for(auto* it = start_ptr; it != end_ptr; ++it) {
        std::uint32_t arg_sz = match_opt(it, str, table.ignore_case);
        if(!arg_sz)
            continue;

        OptionRef opt(*it, table);

        if(options.excludes(opt))
            continue;

        auto a = accept_opt(opt, args, str.substr(0, arg_sz), false, index, out);
        if(a == AcceptResult::Matched) {
            group_buf.clear();
            return static_cast<int>(AcceptResult::Matched);
        }

        if(arg_sz == 2 && opt.kind() == Kind::Flag)
            fallback_opt = it;

        if(prev != index) {
            out.index = prev;
            return static_cast<int>(AcceptResult::MissingValue);
        }
    }

    if(fallback_opt) {
        OptionRef opt(*fallback_opt, table);
        if(str.size() > 2 && str[2] == '=') {
            out.clear();
            out.id = table.unknown_option_id;
            out.spelling = str;
            out.index = index++;
            group_buf.clear();
            return static_cast<int>(AcceptResult::Matched);
        }

        auto a = accept_opt(opt, args, str.substr(0, 2), true, index, out);
        if(a == AcceptResult::Matched) {
            // Save remaining before mutating group_buf, since str may be a
            // view into group_buf and would be invalidated by the assignment.
            auto remaining = str.substr(2);
            group_buf = "-";
            group_buf += remaining;
            return static_cast<int>(AcceptResult::Matched);
        }
    }

    if(str.size() > 1 && str[1] != '-') {
        auto first_flag_name = str.substr(0, 2);
        out.clear();
        out.id = table.unknown_option_id;
        out.spelling = first_flag_name;
        out.index = index;
        if(str.size() > 2) {
            auto remaining = str.substr(2);
            group_buf = "-";
            group_buf += remaining;
        } else {
            group_buf.clear();
            ++index;
        }
        return static_cast<int>(AcceptResult::Matched);
    }

    out.clear();
    out.id = table.unknown_option_id;
    out.spelling = str;
    out.index = index++;
    group_buf.clear();

    if(options.greedy_unknown && str != "--") {
        consume_unknown_values(&table,
                               table.option_infos.data() + search_start,
                               table.option_infos.data() + table.option_infos.size(),
                               table.ignore_case,
                               args,
                               index,
                               out,
                               options);
    }

    return static_cast<int>(AcceptResult::Matched);
}

ParseIter::ParseIter(const OptTable* table,
                     const void* data,
                     std::uint32_t size,
                     ArgAccessFn access,
                     ParseOptions options) :
    table(table), data(data), count(size), access(access), options(options), done(false),
    is_grouped(options.grouped_short_options) {
    advance();
}

void ParseIter::advance() {
    if(done)
        return;

    ArgSpan args(data, count, access);

    while(index < args.size()) {
        auto str = args[index];
        if(str.empty()) {
            ++index;
            continue;
        }

        if(!past_dash_dash && options.dash_dash_parsing && str == "--") {
            past_dash_dash = true;
            if(options.dash_dash_as_single_pack) {
                ParsedArg out;
                out.id = table->input_option_id;
                out.spelling = "--";
                out.index = index;
                for(std::uint32_t i = index + 1; i < args.size(); ++i)
                    out.add_value(args[i]);
                index = args.size();
                out.next_index = index;
                current = out;
                return;
            }
            ++index;
            continue;
        }

        if(past_dash_dash) {
            ParsedArg out;
            out.id = table->input_option_id;
            out.spelling = args[index];
            out.index = index;
            ++index;
            out.next_index = index;
            current = out;
            return;
        }

        if(is_grouped && !in_group) {
            ParsedArg out;
            auto result =
                parse_step_grouped(*table, data, count, access, index, out, group_buf, options);
            if(result == static_cast<int>(AcceptResult::Matched)) {
                if(!group_buf.empty())
                    in_group = true;
                out.next_index = index;
                current = out;
                return;
            }
            if(result == static_cast<int>(AcceptResult::MissingValue)) {
                current = std::unexpected(ParseError{out.index, "missing argument value"});
                return;
            }
        } else if(is_grouped && in_group) {
            ParsedArg out;
            auto buf_access = [](const void* d,
                                 [[maybe_unused]] std::uint32_t i) -> std::string_view {
                return *static_cast<const std::string*>(d);
            };
            std::uint32_t buf_index = 0;
            auto result = parse_step_grouped(*table,
                                             &group_buf,
                                             1,
                                             buf_access,
                                             buf_index,
                                             out,
                                             group_buf,
                                             options);
            out.index = index;
            if(result == static_cast<int>(AcceptResult::Matched)) {
                if(group_buf.empty()) {
                    in_group = false;
                    ++index;
                }
                out.next_index = index;
                current = out;
                return;
            }
            in_group = false;
            ++index;
            continue;
        } else {
            ParsedArg out;
            auto result = parse_step(*table, data, count, access, index, out, options);
            if(result == static_cast<int>(AcceptResult::Matched)) {
                out.next_index = index;
                current = out;
                return;
            }
            if(result == static_cast<int>(AcceptResult::MissingValue)) {
                current = std::unexpected(ParseError{out.index, "missing argument value"});
                return;
            }
        }
    }
    done = true;
}
