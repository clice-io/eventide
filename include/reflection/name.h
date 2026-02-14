#pragma once

#include <source_location>
#include <string_view>
#include <type_traits>

namespace refl::detail {

constexpr std::string_view ltrim(std::string_view sv, std::string_view chars = " \t\n\v\f\r") {
    auto pos = sv.find_first_not_of(chars);
    if(pos == std::string_view::npos) {
        return {};
    }
    return sv.substr(pos);
}

constexpr std::string_view rtrim(std::string_view sv, std::string_view chars = " \t\n\v\f\r") {
    auto pos = sv.find_last_not_of(chars);
    if(pos == std::string_view::npos) {
        return {};
    }
    return sv.substr(0, pos + 1);
}

constexpr std::string_view trim(std::string_view sv, std::string_view chars = " \t\n\v\f\r") {
    return rtrim(ltrim(sv, chars), chars);
}

constexpr std::size_t find_last_top_level_scope(std::string_view sv) {
    std::size_t pos = std::string_view::npos;
    int angle = 0;
    int paren = 0;
    int bracket = 0;
    int brace = 0;

    for(std::size_t i = 0; i + 1 < sv.size(); ++i) {
        const auto ch = sv[i];
        switch(ch) {
            case '<': ++angle; break;
            case '>':
                if(angle > 0) {
                    --angle;
                }
                break;
            case '(': ++paren; break;
            case ')':
                if(paren > 0) {
                    --paren;
                }
                break;
            case '[': ++bracket; break;
            case ']':
                if(bracket > 0) {
                    --bracket;
                }
                break;
            case '{': ++brace; break;
            case '}':
                if(brace > 0) {
                    --brace;
                }
                break;
            default: break;
        }

        if(ch == ':' && sv[i + 1] == ':' && angle == 0 && paren == 0 && bracket == 0 &&
           brace == 0) {
            pos = i;
            ++i;
        }
    }
    return pos;
}

constexpr std::string_view unqualify_type_name(std::string_view sv) {
    if(auto pos = find_last_top_level_scope(sv); pos != std::string_view::npos) {
        return sv.substr(pos + 2);
    }
    return sv;
}

constexpr std::string_view unwrap_wrapper_value(std::string_view sv) {
    auto open = sv.rfind('{');
    auto close = sv.rfind('}');
    if(open != std::string_view::npos && close != std::string_view::npos && open < close) {
        sv = sv.substr(open + 1, close - open - 1);
    }
    return trim(sv);
}

constexpr std::string_view extract_identifier(std::string_view expression) {
    expression = trim(expression);
    while(true) {
        bool changed = false;
        while(!expression.empty() && expression.front() == '&') {
            expression.remove_prefix(1);
            expression = trim(expression);
            changed = true;
        }
        if(expression.size() >= 2 && expression.front() == '(' && expression.back() == ')') {
            expression.remove_prefix(1);
            expression.remove_suffix(1);
            expression = trim(expression);
            changed = true;
        }
        if(!changed) {
            break;
        }
    }

    while(!expression.empty() && (expression.back() == ')' || expression.back() == ']' ||
                                  expression.back() == '}' || expression.back() == ';')) {
        expression.remove_suffix(1);
        expression = trim(expression);
    }

    if(expression.starts_with("::")) {
        expression.remove_prefix(2);
    }
    if(auto pos = expression.rfind("::"); pos != std::string_view::npos) {
        expression = expression.substr(pos + 2);
    }
    if(auto pos = expression.rfind("->"); pos != std::string_view::npos) {
        expression = expression.substr(pos + 2);
    }
    if(auto pos = expression.rfind('.'); pos != std::string_view::npos) {
        expression = expression.substr(pos + 1);
    }
    if(auto pos = expression.find('<'); pos != std::string_view::npos) {
        expression = expression.substr(0, pos);
    }
    if(auto pos = expression.find('('); pos != std::string_view::npos) {
        expression = expression.substr(0, pos);
    }

    return trim(expression);
}

/// workaround for msvc, if no such wrapper, msvc cannot print the member name.
template <typename T>
struct wrapper {
    T value;

    constexpr wrapper(T value) : value(value) {}
};

}  // namespace refl::detail

namespace refl {

template <typename T>
consteval auto type_name(bool qualified = false) {
    std::string_view name = std::source_location::current().function_name();
#if __GNUC__ || __clang__
    std::size_t start = name.rfind("T =") + 3;
    std::size_t end = name.rfind("]");
    end = end == std::string_view::npos ? name.size() : end;
    name = detail::trim(name.substr(start, end - start));
#elif _MSC_VER
    std::size_t start = name.find("type_name<") + 10;
    std::size_t end = name.rfind(">(");
    name = std::string_view{name.data() + start, end - start};
    start = name.find(' ');
    name = start == std::string_view::npos
               ? name
               : std::string_view(name.data() + start + 1, name.size() - start - 1);
#endif
    if(!qualified) {
        name = detail::unqualify_type_name(name);
    }
    return name;
}

template <auto value>
    requires std::is_enum_v<decltype(value)>
consteval auto enum_name() {
    std::string_view name = std::source_location::current().function_name();
#if __GNUC__ || __clang__
    std::size_t start = name.find('=') + 2;
    std::size_t end = name.size() - 1;
#elif _MSC_VER
    std::size_t start = name.find('<') + 1;
    std::size_t end = name.rfind(">(");
#else
    static_assert(false, "Not supported compiler");
#endif
    name = detail::trim(name.substr(start, end - start));
    start = name.rfind("::");
    return start == std::string_view::npos ? name : name.substr(start + 2);
}

template <detail::wrapper ptr>
    requires std::is_pointer_v<decltype(ptr.value)>
consteval auto pointer_name() {
    std::string_view name = std::source_location::current().function_name();
    name = detail::unwrap_wrapper_value(name);
    return detail::extract_identifier(name);
}

template <detail::wrapper ptr>
    requires std::is_member_pointer_v<decltype(ptr.value)>
consteval auto member_name() {
    std::string_view name = std::source_location::current().function_name();
    name = detail::unwrap_wrapper_value(name);
    return detail::extract_identifier(name);
}

}  // namespace refl
