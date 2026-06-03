#pragma once

#include <cassert>
#include <expected>
#include <iterator>
#include <ostream>
#include <ranges>
#include <set>
#include <span>
#include <string_view>
#include <vector>

#include "arg.h"

namespace kota::option {

class OptTable;

class Option {
protected:
    const struct OptTableInfo* info;
    const OptTable* owner;

public:
    Option(const OptTableInfo* info, const OptTable* owner);

    bool valid() const {
        return this->info != nullptr;
    }

    std::uint32_t id() const;
    Kind kind() const;
    std::string_view name() const;
    const Option group() const;
    const Option alias() const;
    const char* alias_args() const;
    std::string_view prefix() const;
    std::string_view prefixed_name() const;
    std::string_view help_text() const;
    std::string_view meta_var() const;
    std::uint32_t num_args() const;

    bool has_no_opt_as_input() const;
    RenderStyle render_style() const;
    bool has_flag(std::uint32_t val) const;
    bool has_visibility_flag(std::uint32_t val) const;

    const Option unaliased_option() const {
        const Option als = this->alias();
        if(als.valid())
            return als.unaliased_option();
        return *this;
    }

    std::string_view render_name() const {
        return this->unaliased_option().name();
    }

    bool matches(std::uint32_t opt_id) const;

    void print(std::ostream& o, bool add_new_line) const;
};

struct OptFilter {
    std::uint32_t visibility = ~0U;
    std::uint32_t include_flags = 0;
    std::uint32_t exclude_flags = 0;

    OptFilter() = default;

    OptFilter(std::uint32_t vis) : visibility(vis) {}

    OptFilter(std::uint32_t vis, std::uint32_t include, std::uint32_t exclude) :
        visibility(vis), include_flags(include), exclude_flags(exclude) {}

    bool excludes(const Option& opt) const;
};

using ArgAccessFn = std::string_view (*)(const void* data, std::uint32_t index);

struct OptTableInfo {
    std::span<const std::string_view> prefixes;
    std::string_view prefixed_name;
    std::uint32_t id;
    Kind kind;
    unsigned short group_id;
    unsigned short alias_id;
    const char* alias_args;
    std::uint32_t flags;
    std::uint32_t visibility;
    unsigned char num_args;
    const char* help_text;
    const char* meta_var;

    bool has_no_prefix() const {
        return this->prefixes.size() == 0;
    }

    std::string_view name() const {
        std::uint32_t prefix_length =
            this->has_no_prefix() ? 0 : static_cast<std::uint32_t>(this->prefixes[0].size());
        return this->prefixed_name.substr(prefix_length);
    }

    constexpr static OptTableInfo unaliased_one(std::span<const std::string_view> prefixes,
                                                std::string_view prefixed_name,
                                                std::uint32_t id,
                                                Kind kind,
                                                unsigned char num_args,
                                                const char* help_text = "no help text",
                                                const char* meta_var = "<nullptr>",
                                                unsigned short group_id = 0,
                                                std::uint32_t flags = 0,
                                                std::uint32_t visibility = 1 << 0) {
        return OptTableInfo{
            .prefixes = prefixes,
            .prefixed_name = prefixed_name,
            .id = id,
            .kind = kind,
            .group_id = group_id,
            .alias_id = 0,
            .alias_args = nullptr,
            .flags = flags,
            .visibility = visibility,
            .num_args = num_args,
            .help_text = help_text,
            .meta_var = meta_var,
        };
    };

    constexpr static OptTableInfo unknown(std::uint32_t id) {
        return OptTableInfo::unaliased_one(pfx_none,
                                           "<unknown>",
                                           id,
                                           Kind::Unknown,
                                           0,
                                           "Unknown option");
    }

    constexpr static OptTableInfo input(std::uint32_t id) {
        return OptTableInfo::unaliased_one(pfx_none,
                                           "<input>",
                                           id,
                                           Kind::Input,
                                           0,
                                           "input content");
    }

    constexpr auto alias_of(std::uint32_t origin_id, const char* origin_args = nullptr) {
        this->alias_args = origin_args;
        this->alias_id = static_cast<unsigned short>(origin_id);
        return *this;
    }
};

class ParseIter;

class OptTable {
public:
    using Info = OptTableInfo;

    OptTable(std::span<const Info> option_infos,
             bool ignore_case = false,
             std::vector<std::string_view> prefixes_union = {},
             bool build_now = true);

    OptTable& build();

    virtual ~OptTable() = default;

    std::uint32_t num_options() const {
        return static_cast<std::uint32_t>(this->option_infos.size());
    }

    auto options() const {
        return this->option_infos;
    }

    const Option option(std::uint32_t opt_id) const;

    auto set_grouped_short_options(bool value) {
        this->grouped_short_options = value;
        return *this;
    }

    auto set_dash_dash_parsing(bool value) {
        this->dash_dash_parsing = value;
        return *this;
    }

    auto set_dash_dash_as_single_pack(bool value) {
        this->dash_dash_as_single_pack = value;
        return *this;
    }

    auto set_tablegen_mode(bool value) {
        this->tablegen_mode = value;
        return *this;
    }

    auto set_input_random_index(bool value) {
        this->input_random_index = value;
        return *this;
    }

    auto set_ignore_case(bool value) {
        this->ignore_case = value;
        return *this;
    }

    std::span<const std::string_view> prefixes_union() const {
        return std::span(this->_prefixes_union);
    }

    std::uint32_t find_option(std::string_view argument, OptFilter filter = {}) const;

    template <typename Range>
    auto parse(Range& argv, OptFilter filter = {}) const;

private:
    friend class ParseIter;

    int parse_step(const void* data,
                   std::uint32_t size,
                   ArgAccessFn access,
                   std::uint32_t& index,
                   ParsedArg& out,
                   OptFilter filter = {}) const;

    int parse_step_grouped(const void* data,
                           std::uint32_t size,
                           ArgAccessFn access,
                           std::uint32_t& index,
                           ParsedArg& out,
                           std::string& group_buf,
                           OptFilter filter = {}) const;

    const Info& info(std::uint32_t opt_id) const {
        assert(opt_id > 0 && opt_id - 1 < this->num_options() && "Invalid Option ID.");
        return this->option_infos[opt_id - 1];
    }

    void buildPrefixChars() {
        assert(this->prefix_chars.empty() && "rebuilding a non-empty prefix char");
        std::set<char> seen_chars;
        for(auto& prefix: this->_prefixes_union) {
            seen_chars.insert(prefix.begin(), prefix.end());
        }
        this->prefix_chars.assign(seen_chars.begin(), seen_chars.end());
    }

    std::span<const Info> option_infos;
    bool ignore_case;
    bool grouped_short_options = false;
    bool dash_dash_parsing = false;
    bool input_random_index = false;
    bool dash_dash_as_single_pack = false;
    std::uint32_t input_option_id = 0;
    std::uint32_t unknown_option_id = 0;
    bool tablegen_mode = false;

protected:
    std::uint32_t first_searchable_index = 0;
    std::vector<std::string_view> _prefixes_union;
    std::vector<char> prefix_chars;
};

/// Yields ParsedArg views into the argv range; the range must outlive iteration.
class ParseIter {
public:
    using iterator_concept = std::input_iterator_tag;
    using value_type = std::expected<ParsedArg, ParseError>;
    using difference_type = std::ptrdiff_t;

    ParseIter() = default;

    ParseIter(const OptTable* table,
              const void* data,
              std::uint32_t size,
              ArgAccessFn access,
              OptFilter filter);

    const value_type& operator*() const {
        return current;
    }

    const value_type* operator->() const {
        return &current;
    }

    ParseIter& operator++() {
        advance();
        return *this;
    }

    ParseIter operator++(int) {
        ParseIter tmp = *this;
        advance();
        return tmp;
    }

    bool operator==(const ParseIter& other) const {
        return done && other.done;
    }

    bool operator!=(const ParseIter& other) const {
        return !(*this == other);
    }

private:
    void advance();

    const OptTable* table = nullptr;
    const void* data = nullptr;
    std::uint32_t count = 0;
    ArgAccessFn access = nullptr;
    std::uint32_t index = 0;
    OptFilter filter;
    bool done = true;
    value_type current;
    bool is_dash_dash_parsing = false;
    bool is_dash_dash_as_single_pack = false;
    bool is_grouped = false;
    std::string group_buf;
    bool in_group = false;
    bool past_dash_dash = false;
};

template <typename Range>
auto OptTable::parse(Range& argv, OptFilter filter) const {
    using Elem = std::remove_cvref_t<decltype(argv[0])>;
    auto access = [](const void* d, std::uint32_t i) -> std::string_view {
        return static_cast<const Elem*>(d)[i];
    };
    return std::ranges::subrange(
        ParseIter(this, argv.data(), static_cast<std::uint32_t>(argv.size()), access, filter),
        ParseIter());
}

}  // namespace kota::option
