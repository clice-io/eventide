#include "kota/zest/snap.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <sstream>
#include <string>

#include "kota/codec/content/document.h"

namespace fs = std::filesystem;

namespace {

constexpr std::string_view snap_dir_name = "__snapshots__";
constexpr std::string_view snap_ext = ".snap";
constexpr std::string_view yellow = "\033[33m";
constexpr std::string_view red = "\033[31m";
constexpr std::string_view green = "\033[32m";
constexpr std::string_view cyan = "\033[36m";
constexpr std::string_view clear = "\033[0m";

bool should_update_snapshots() {
    static const bool update = [] {
        const char* env = std::getenv("ZEST_UPDATE_SNAPSHOTS");
        return env != nullptr && std::string_view(env) == "1";
    }();
    return update;
}

std::string read_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if(!file) {
        return {};
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool write_file(const fs::path& path, std::string_view content) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if(!file) {
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
}

bool matches_glob_pattern(std::string_view text, std::string_view pattern) {
    std::size_t ti = 0, pi = 0, star = std::string_view::npos, match = 0;
    while(ti < text.size()) {
        if(pi < pattern.size() && (pattern[pi] == text[ti])) {
            ++ti;
            ++pi;
        } else if(pi < pattern.size() && pattern[pi] == '*') {
            star = pi++;
            match = ti;
        } else if(star != std::string_view::npos) {
            pi = star + 1;
            ti = ++match;
        } else {
            return false;
        }
    }
    while(pi < pattern.size() && pattern[pi] == '*') {
        ++pi;
    }
    return pi == pattern.size();
}

void print_diff(std::string_view expected, std::string_view actual) {
    std::println("{}  --- expected{}", red, clear);
    std::println("{}  +++ actual{}", green, clear);

    auto lines_of = [](std::string_view s) {
        std::vector<std::string_view> lines;
        while(!s.empty()) {
            auto pos = s.find('\n');
            if(pos == std::string_view::npos) {
                lines.push_back(s);
                break;
            }
            lines.push_back(s.substr(0, pos));
            s.remove_prefix(pos + 1);
        }
        return lines;
    };

    auto expected_lines = lines_of(expected);
    auto actual_lines = lines_of(actual);

    auto max_lines = std::max(expected_lines.size(), actual_lines.size());
    for(std::size_t i = 0; i < max_lines; ++i) {
        bool have_exp = i < expected_lines.size();
        bool have_act = i < actual_lines.size();
        if(have_exp && have_act && expected_lines[i] == actual_lines[i]) {
            std::println("   {}", expected_lines[i]);
        } else {
            if(have_exp) {
                std::println("{}  - {}{}", red, expected_lines[i], clear);
            }
            if(have_act) {
                std::println("{}  + {}{}", green, actual_lines[i], clear);
            }
        }
    }
}

void emit_yaml_impl(std::string& out,
                     const kota::codec::content::Value& val,
                     int depth,
                     bool is_list_item) {
    using namespace kota::codec::content;

    auto indent = [&](int extra = 0) {
        out.append(static_cast<std::size_t>((depth + extra) * 2), ' ');
    };

    switch(val.kind()) {
        case ValueKind::null_value: out += "null"; break;
        case ValueKind::boolean: out += val.as_bool() ? "true" : "false"; break;
        case ValueKind::signed_int: out += std::to_string(val.as_int()); break;
        case ValueKind::unsigned_int: out += std::to_string(val.as_uint()); break;
        case ValueKind::floating: {
            auto d = val.as_double();
            auto s = std::to_string(d);
            // trim trailing zeros but keep at least one decimal
            auto dot = s.find('.');
            if(dot != std::string::npos) {
                auto last_nonzero = s.find_last_not_of('0');
                if(last_nonzero != std::string::npos && last_nonzero > dot) {
                    s.erase(last_nonzero + 1);
                } else {
                    s.erase(dot + 2);
                }
            }
            out += s;
            break;
        }
        case ValueKind::string: {
            auto sv = val.as_string();
            bool needs_quote = sv.empty() || sv.find_first_of(":#{}[]&*?|>!%@`,\n\"'") != std::string_view::npos;
            if(!needs_quote) {
                // check if it looks like a bool/null/number
                if(sv == "true" || sv == "false" || sv == "null" || sv == "~" ||
                   sv == "yes" || sv == "no" || sv == "on" || sv == "off") {
                    needs_quote = true;
                }
            }
            if(needs_quote) {
                out += '"';
                for(char c : sv) {
                    switch(c) {
                        case '"': out += "\\\""; break;
                        case '\\': out += "\\\\"; break;
                        case '\n': out += "\\n"; break;
                        case '\t': out += "\\t"; break;
                        case '\r': out += "\\r"; break;
                        default: out += c;
                    }
                }
                out += '"';
            } else {
                out += sv;
            }
            break;
        }
        case ValueKind::array: {
            const auto& arr = val.as_array();
            if(arr.empty()) {
                out += "[]";
                break;
            }
            for(std::size_t i = 0; i < arr.size(); ++i) {
                if(i > 0 || (depth > 0 && !is_list_item)) {
                    out += '\n';
                    indent();
                }
                out += "- ";
                bool child_compound = arr[i].is_object() || arr[i].is_array();
                emit_yaml_impl(out, arr[i], depth + 1, true);
                if(!child_compound && i < arr.size() - 1) {
                    // scalar items are already complete
                }
            }
            break;
        }
        case ValueKind::object: {
            const auto& obj = val.as_object();
            if(obj.empty()) {
                out += "{}";
                break;
            }
            std::size_t i = 0;
            for(const auto& [key, value] : obj) {
                if(i > 0 || (depth > 0 && !is_list_item)) {
                    out += '\n';
                    indent();
                }
                out += key;
                out += ':';
                if(value.is_object() || value.is_array()) {
                    emit_yaml_impl(out, value, depth + 1, false);
                } else {
                    out += ' ';
                    emit_yaml_impl(out, value, depth + 1, false);
                }
                ++i;
            }
            break;
        }
    }
}

}  // namespace

namespace kota::zest::snap {

void reset_counter() {
    current_test_context().snap_counter = 0;
}

std::uint32_t next_counter() {
    return current_test_context().snap_counter++;
}

std::optional<GlobContext>& current_glob_context() {
    static thread_local std::optional<GlobContext> ctx;
    return ctx;
}

fs::path snapshot_path(std::string_view source_file,
                       std::string_view suite,
                       std::string_view test,
                       std::uint32_t counter,
                       const std::optional<GlobContext>& glob_ctx) {
    auto source_dir = fs::path(source_file).parent_path();
    auto snap_dir = source_dir / snap_dir_name;

    std::string filename;
    filename += suite;
    filename += "__";
    filename += test;
    if(glob_ctx.has_value()) {
        filename += "__";
        filename += glob_ctx->stem;
    }
    if(counter > 0) {
        filename += '@';
        filename += std::to_string(counter + 1);
    }
    filename += snap_ext;
    return snap_dir / filename;
}

Result check(std::string_view content,
             std::string_view expr,
             std::source_location loc) {
    auto& ctx = current_test_context();
    auto counter = next_counter();
    auto& glob_ctx = current_glob_context();

    auto path = snapshot_path(ctx.file, ctx.suite, ctx.test, counter, glob_ctx);

    if(should_update_snapshots()) {
        write_file(path, content);
        std::println("{}[ snap  ] updated: {}{}", yellow, path.string(), clear);
        return Result::updated;
    }

    if(!fs::exists(path)) {
        write_file(path, content);
        std::println("{}[ snap  ] created: {}{}", cyan, path.string(), clear);
        return Result::created;
    }

    auto expected = read_file(path);
    if(expected == content) {
        return Result::matched;
    }

    std::println("[ snap  ] snapshot mismatch for: {}", expr);
    std::println("           file: {}", path.string());
    std::println("           at {}:{}", loc.file_name(), loc.line());
    print_diff(expected, std::string(content));
    return Result::mismatch;
}

bool check_inline(std::string_view actual,
                  std::string_view expected,
                  std::string_view expr,
                  std::source_location loc) {
    if(actual == expected) {
        return true;
    }

    std::println("[ snap  ] inline snapshot mismatch for: {}", expr);
    std::println("           at {}:{}", loc.file_name(), loc.line());
    print_diff(expected, actual);
    return false;
}

std::string prettify_json(std::string_view json) {
    std::string out;
    out.reserve(json.size() * 2);
    int depth = 0;
    bool in_string = false;

    auto indent_newline = [&] {
        out += '\n';
        out.append(static_cast<std::size_t>(depth * 2), ' ');
    };

    for(std::size_t i = 0; i < json.size(); ++i) {
        char c = json[i];

        if(in_string) {
            out += c;
            if(c == '\\' && i + 1 < json.size()) {
                out += json[++i];
            } else if(c == '"') {
                in_string = false;
            }
            continue;
        }

        switch(c) {
            case '"': in_string = true; out += c; break;
            case '{':
            case '[':
                out += c;
                if(i + 1 < json.size() && (json[i + 1] == '}' || json[i + 1] == ']')) {
                    out += json[++i];
                } else {
                    ++depth;
                    indent_newline();
                }
                break;
            case '}':
            case ']': --depth; indent_newline(); out += c; break;
            case ',': out += ','; indent_newline(); break;
            case ':': out += ": "; break;
            case ' ':
            case '\t':
            case '\n':
            case '\r': break;
            default: out += c;
        }
    }
    return out;
}

std::string value_to_yaml(const codec::content::Value& value) {
    std::string out;
    emit_yaml_impl(out, value, 0, false);
    out += '\n';
    return out;
}

void glob(std::string_view pattern,
          std::string_view source_file,
          std::function<void(const fs::path&)> callback) {
    auto source_dir = fs::path(source_file).parent_path();

    std::string_view dir_part;
    std::string_view file_pattern;
    auto last_slash = pattern.rfind('/');
    if(last_slash != std::string_view::npos) {
        dir_part = pattern.substr(0, last_slash);
        file_pattern = pattern.substr(last_slash + 1);
    } else {
        file_pattern = pattern;
    }

    auto search_dir = dir_part.empty() ? source_dir : source_dir / std::string(dir_part);

    if(!fs::exists(search_dir) || !fs::is_directory(search_dir)) {
        std::println("{}[ snap  ] glob: directory not found: {}{}", red, search_dir.string(), clear);
        return;
    }

    std::vector<fs::path> matched;
    for(const auto& entry : fs::directory_iterator(search_dir)) {
        if(!entry.is_regular_file()) {
            continue;
        }
        auto filename = entry.path().filename().string();
        if(matches_glob_pattern(filename, file_pattern)) {
            matched.push_back(entry.path());
        }
    }

    std::sort(matched.begin(), matched.end());

    auto& glob_ctx = current_glob_context();
    for(const auto& path : matched) {
        glob_ctx = GlobContext{path.stem().string()};
        reset_counter();
        callback(path);
    }
    glob_ctx.reset();
}

}  // namespace kota::zest::snap
