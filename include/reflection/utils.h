#pragma once

#include <string_view>

namespace refl::utils {

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

constexpr bool consume_front(std::string_view& sv, std::string_view prefix) {
    if(!sv.starts_with(prefix)) {
        return false;
    }
    sv.remove_prefix(prefix.size());
    return true;
}

constexpr bool consume_back(std::string_view& sv, std::string_view suffix) {
    if(!sv.ends_with(suffix)) {
        return false;
    }
    sv.remove_suffix(suffix.size());
    return true;
}

constexpr std::string_view take_front(std::string_view sv, std::size_t n) {
    if(n >= sv.size()) {
        return sv;
    }
    return sv.substr(0, n);
}

constexpr std::string_view take_back(std::string_view sv, std::size_t n) {
    if(n >= sv.size()) {
        return sv;
    }
    return sv.substr(sv.size() - n, n);
}

constexpr std::string_view drop_front(std::string_view sv, std::size_t n = 1) {
    if(n >= sv.size()) {
        return {};
    }
    return sv.substr(n);
}

constexpr std::string_view drop_back(std::string_view sv, std::size_t n = 1) {
    if(n >= sv.size()) {
        return {};
    }
    return sv.substr(0, sv.size() - n);
}

}  // namespace refl::utils
