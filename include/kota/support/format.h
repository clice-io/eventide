#pragma once

#include <format>
#include <string>
#include <utility>

namespace kota {

/// std::format stand-in for kotatsu headers. Template bodies here get
/// instantiated inside consumers' C++20 module units, where overload
/// resolution on std::format completes the wide basic_format_string; clang 22
/// demotes its serialized member definitions and libc++ hard-errors the
/// re-instantiation (llvm/llvm-project#174858, fixed in clang 23 by
/// PR#184287 — fold this back to std::format afterwards). vformat's overloads
/// take concrete parameter types, so nothing wchar_t is ever completed; the
/// format_string parameter keeps the compile-time format check.
template <typename... Args>
[[nodiscard]] inline std::string fmt(std::format_string<Args...> f, Args&&... args) {
    return std::vformat(f.get(), std::make_format_args(args...));
}

}  // namespace kota
