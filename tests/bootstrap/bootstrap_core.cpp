#undef NDEBUG
#include <cassert>
#include <chrono>
#include <print>
#include <string>

#include "worker.h"
#include "kota/zest/assert/check.h"
#include "kota/zest/runner/registry.h"
#include "kota/zest/runner/suite.h"
#include "kota/support/glob_pattern.h"

using namespace kota;
using namespace kota::zest;
using namespace kota::zest::detail;

int main() {
    std::println("--- worker protocol ---");

    {
        struct Case {
            TestState state;
            std::chrono::milliseconds dur;
        };

        Case cases[] = {
            {TestState::Passed,  std::chrono::milliseconds(0)  },
            {TestState::Skipped, std::chrono::milliseconds(42) },
            {TestState::Failed,  std::chrono::milliseconds(100)},
            {TestState::Fatal,   std::chrono::milliseconds(999)},
        };

        for(const auto& c: cases) {
            auto line = format_result_line(c.state, c.dur);
            TestState parsed_state{};
            std::chrono::milliseconds parsed_dur{};
            bool ok = parse_result_line(line, parsed_state, parsed_dur);
            assert(ok);
            assert(parsed_state == c.state);
            assert(parsed_dur == c.dur);
        }
    }

    {
        TestState s{};
        std::chrono::milliseconds d{};
        assert(!parse_result_line("", s, d));
    }

    {
        TestState s{};
        std::chrono::milliseconds d{};
        assert(!parse_result_line("some random line", s, d));
    }

    {
        TestState s{};
        std::chrono::milliseconds d{};
        assert(!parse_result_line("__ZEST_RESULT__:passed", s, d));
    }

    {
        auto line = format_result_line(TestState::Passed, std::chrono::milliseconds(50));
        auto line_with_newline = line + "\n";
        TestState s{};
        std::chrono::milliseconds d{};
        bool ok = parse_result_line(line_with_newline, s, d);
        assert(ok);
        assert(s == TestState::Passed);
        assert(d == std::chrono::milliseconds(50));
    }

    {
        auto line0 = format_result_line(TestState::Passed, std::chrono::milliseconds(0));
        assert(line0 == "__ZEST_RESULT__:passed:0");

        auto line999 = format_result_line(TestState::Fatal, std::chrono::milliseconds(999));
        assert(line999 == "__ZEST_RESULT__:fatal:999");
    }

    {
        TestState s{};
        std::chrono::milliseconds d{};
        assert(!parse_result_line("__ZEST_RESULT__:passed:123abc", s, d));
    }

    {
        TestState s{};
        std::chrono::milliseconds d{};
        assert(!parse_result_line("__ZEST_RESULT__:unknown:42", s, d));
    }

    std::println("--- glob pattern ---");

    {
        auto pat = GlobPattern::create("*.txt");
        assert(pat.has_value());
    }

    {
        auto pat = GlobPattern::create("*.txt");
        assert(pat->match("hello.txt"));
        assert(pat->match("a.txt"));
        assert(!pat->match("dir/hello.txt"));
    }

    {
        auto pat = GlobPattern::create("**/*.txt");
        assert(pat->match("dir/hello.txt"));
        assert(pat->match("a/b/c.txt"));
    }

    {
        auto pat = GlobPattern::create("*");
        assert(pat->is_trivial_match_all());
    }

    {
        auto pat = GlobPattern::create("**");
        assert(pat->is_trivial_match_all());
    }

    {
        auto pat = GlobPattern::create("*.txt");
        assert(!pat->is_trivial_match_all());
    }

    {
        auto pat = GlobPattern::create("[unclosed");
        assert(!pat.has_value());
    }

    {
        auto pat = GlobPattern::create("?.txt");
        assert(pat->match("a.txt"));
        assert(!pat->match("ab.txt"));
    }

    {
        auto pat = GlobPattern::create("src/**/*.{h,cpp}");
        assert(pat.has_value());
        assert(pat->match("src/foo/bar.h"));
        assert(pat->match("src/foo/bar.cpp"));
        assert(!pat->match("src/foo/bar.py"));
    }

    std::println("--- attrs ---");

    {
        TestAttrs a{};
        assert(!a.skip);
        assert(!a.focus);
        assert(!a.serial);
    }

    {
        auto r = merge_attrs({}, {});
        assert(!r.skip);
        assert(!r.focus);
        assert(!r.serial);
    }

    {
        auto r = merge_attrs({.skip = true}, {});
        assert(r.skip);
        assert(!r.focus);
        assert(!r.serial);
    }

    {
        auto r = merge_attrs({}, {.focus = true});
        assert(!r.skip);
        assert(r.focus);
        assert(!r.serial);
    }

    {
        auto r = merge_attrs({.serial = true}, {.focus = true});
        assert(!r.skip);
        assert(r.focus);
        assert(r.serial);
    }

    {
        auto r = merge_attrs({.skip = true, .focus = true, .serial = true}, {});
        assert(r.skip);
        assert(r.focus);
        assert(r.serial);
    }

    {
        static_assert(static_cast<int>(TestState::Passed) == 0);
        static_assert(static_cast<int>(TestState::Skipped) == 1);
        static_assert(static_cast<int>(TestState::Failed) == 2);
        static_assert(static_cast<int>(TestState::Fatal) == 3);
    }

    std::println("--- expression parsing ---");

    {
        auto [lhs, rhs] = parse_binary_exprs("a, b");
        assert(lhs == "a");
        assert(rhs == "b");
    }

    {
        auto [lhs, rhs] = parse_binary_exprs("  a  ,  b  ");
        assert(lhs == "a");
        assert(rhs == "b");
    }

    {
        auto [lhs, rhs] = parse_binary_exprs("a");
        assert(lhs == "a");
        assert(rhs == "<unknown>");
    }

    {
        auto [lhs, rhs] = parse_binary_exprs("f<int, float>(x), y");
        assert(lhs == "f<int, float>(x)");
        assert(rhs == "y");
    }

    {
        auto [lhs, rhs] = parse_binary_exprs("a[1,2], b");
        assert(lhs == "a[1,2]");
        assert(rhs == "b");
    }

    {
        auto [lhs, rhs] = parse_binary_exprs("f(a, b), c");
        assert(lhs == "f(a, b)");
        assert(rhs == "c");
    }

    {
        auto [lhs, rhs] = parse_binary_exprs("m{1, 2}, n");
        assert(lhs == "m{1, 2}");
        assert(rhs == "n");
    }

    std::println("all bootstrap_core tests passed");
    return 0;
}
