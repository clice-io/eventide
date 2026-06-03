#include "kota/deco/option.h"

#include <cassert>
#include <ostream>
#include <string_view>
#include <utility>

namespace kota::option {

Option::Option(const OptTableInfo* info, const OptTable* owner) : info(info), owner(owner) {
    assert((!info || !this->alias().valid() || !this->alias().alias().valid()) &&
           "Multi-level aliases are not supported.");

    if(info && this->alias_args()) {
        assert(this->alias().valid() && "Only alias options can have alias args.");
        assert(this->kind() == Kind::Flag && "Only Flag aliases can have alias args.");
        assert(this->alias().kind() != Kind::Flag && "Cannot provide alias args to a flag option.");
    }
}

std::uint32_t Option::id() const {
    assert(this->info && "Must have a valid info!");
    return this->info->id;
}

Kind Option::kind() const {
    assert(this->info && "Must have a valid info!");
    return this->info->kind;
}

std::string_view Option::name() const {
    assert(this->info && "Must have a valid info!");
    return this->info->name();
}

const Option Option::group() const {
    assert(this->info && "Must have a valid info!");
    assert(this->owner && "Must have a valid owner!");
    return this->owner->option(this->info->group_id);
}

const Option Option::alias() const {
    assert(this->info && "Must have a valid info!");
    assert(this->owner && "Must have a valid owner!");
    return this->owner->option(this->info->alias_id);
}

const char* Option::alias_args() const {
    assert(this->info && "Must have a valid info!");
    assert((!this->info->alias_args || this->info->alias_args[0] != 0) &&
           "AliasArgs should be either 0 or non-empty.");
    return this->info->alias_args;
}

std::string_view Option::prefix() const {
    assert(this->info && "Must have a valid info!");
    return this->info->has_no_prefix() ? "" : this->info->prefixes[0];
}

std::string_view Option::prefixed_name() const {
    assert(this->info && "Must have a valid info!");
    return this->info->prefixed_name;
}

std::string_view Option::help_text() const {
    assert(this->info && "Must have a valid info!");
    return this->info->help_text;
}

std::string_view Option::meta_var() const {
    assert(this->info && "Must have a valid info!");
    return this->info->meta_var;
}

std::uint32_t Option::num_args() const {
    assert(this->info && "Must have a valid info!");
    return this->info->num_args;
}

bool Option::has_no_opt_as_input() const {
    assert(this->info && "Must have a valid info!");
    return this->info->flags & RenderAsInput;
}

RenderStyle Option::render_style() const {
    assert(this->info && "Must have a valid info!");
    if(this->info->flags & RenderJoined)
        return RenderStyle::Joined;
    if(this->info->flags & RenderSeparate)
        return RenderStyle::Separate;
    switch(this->kind()) {
        case Kind::Group:
        case Kind::Input:
        case Kind::Unknown: return RenderStyle::Values;
        case Kind::Joined:
        case Kind::JoinedAndSeparate: return RenderStyle::Joined;
        case Kind::CommaJoined: return RenderStyle::CommaJoined;
        case Kind::Flag:
        case Kind::Values:
        case Kind::Separate:
        case Kind::MultiArg:
        case Kind::JoinedOrSeparate:
        case Kind::RemainingArgs:
        case Kind::RemainingArgsJoined: return RenderStyle::Separate;
    }
    std::unreachable();
}

bool Option::has_flag(std::uint32_t val) const {
    assert(this->info && "Must have a valid info!");
    return this->info->flags & val;
}

bool Option::has_visibility_flag(std::uint32_t val) const {
    assert(this->info && "Must have a valid info!");
    return this->info->visibility & val;
}

void Option::print(std::ostream& o, bool add_new_line) const {
    o << "<";
    switch(this->kind()) {
#define P(N)                                                                                       \
    case N: o << #N; break
        P(Kind::Group);
        P(Kind::Input);
        P(Kind::Unknown);
        P(Kind::Flag);
        P(Kind::Joined);
        P(Kind::Values);
        P(Kind::Separate);
        P(Kind::CommaJoined);
        P(Kind::MultiArg);
        P(Kind::JoinedOrSeparate);
        P(Kind::JoinedAndSeparate);
        P(Kind::RemainingArgs);
        P(Kind::RemainingArgsJoined);
#undef P
    }

    if(!this->info->has_no_prefix()) {
        o << " Prefixes:[";
        for(size_t i = 0, n = this->info->prefixes.size(); i != n; ++i)
            o << '"' << this->info->prefixes[i] << (i == n - 1 ? "\"" : "\", ");
        o << ']';
    }

    o << " Name:\"" << this->name() << '"';

    const Option g = this->group();
    if(g.valid()) {
        o << " Group:";
        g.print(o, false);
    }

    const Option als = this->alias();
    if(als.valid()) {
        o << " Alias:";
        als.print(o, false);
    }

    if(this->kind() == Kind::MultiArg)
        o << " NumArgs:" << this->num_args();

    o << ">";
    if(add_new_line) {
        o << "\n";
    }
}

bool Option::matches(std::uint32_t opt_id) const {
    const Option als = this->alias();
    if(als.valid()) {
        return als.matches(opt_id);
    }

    if(this->id() == opt_id) {
        return true;
    }

    const Option g = this->group();
    if(g.valid()) {
        return g.matches(opt_id);
    }
    return false;
}

}  // namespace kota::option
