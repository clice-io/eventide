#include <array>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kota/deco/option.h"
#include "kota/zest/zest.h"

namespace kota::option {
namespace {

using namespace std::literals::string_view_literals;

std::vector<std::string> split2vec(std::string_view str) {
    auto views = std::views::split(str, ' ') | std::views::transform([](auto&& rng) {
                     return std::string(rng.begin(), rng.end());
                 });
    return std::vector<std::string>(views.begin(), views.end());
}

struct ParseCapture {
    std::vector<std::string> argv;
    std::vector<ParsedArg> args;
    std::vector<ParseError> errors;
};

ParseCapture parse_all(const OptTable& table, std::vector<std::string> argv) {
    ParseCapture capture;
    capture.argv = std::move(argv);
    for(auto& result: table.parse(capture.argv)) {
        if(result.has_value()) {
            capture.args.push_back(*result);
        } else {
            capture.errors.push_back(result.error());
        }
    }
    return capture;
}

enum MainOptionID {
    MAIN_OPT_INVALID = 0,
    MAIN_OPT_INPUT = 1,
    MAIN_OPT_UNKNOWN = 2,
    MAIN_OPT_HELP,
    MAIN_OPT_HELP_SHORT,
    MAIN_OPT_SCRIPT,
};

constexpr auto kMainOptInfos = std::array{
    OptTableInfo::input(MAIN_OPT_INPUT),
    OptTableInfo::unknown(MAIN_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_double,
                                "--help",
                                MAIN_OPT_HELP,
                                Kind::Flag,
                                0,
                                "Display help",
                                ""),
    OptTableInfo::unaliased_one(pfx_dash,
                                "-h",
                                MAIN_OPT_HELP_SHORT,
                                Kind::Flag,
                                0,
                                "Display help",
                                "")
        .alias_of(MAIN_OPT_HELP),
    OptTableInfo::unaliased_one(pfx_dash,
                                "-s",
                                MAIN_OPT_SCRIPT,
                                Kind::Separate,
                                1,
                                "Script path",
                                ""),
};

OptTable make_main_opt_table() {
    return OptTable(std::span<const OptTableInfo>(kMainOptInfos))
        .set_tablegen_mode(false)
        .set_dash_dash_parsing(true)
        .set_dash_dash_as_single_pack(true);
}

enum ProxyOptionID {
    PROXY_OPT_INVALID = 0,
    PROXY_OPT_INPUT = 1,
    PROXY_OPT_UNKNOWN = 2,
    PROXY_OPT_PARENT_ID,
    PROXY_OPT_EXEC,
};

constexpr auto kProxyOptInfos = std::array{
    OptTableInfo::input(PROXY_OPT_INPUT),
    OptTableInfo::unknown(PROXY_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_dash,
                                "-p",
                                PROXY_OPT_PARENT_ID,
                                Kind::Separate,
                                1,
                                "Parent process id",
                                ""),
    OptTableInfo::unaliased_one(pfx_dash_double,
                                "--exec",
                                PROXY_OPT_EXEC,
                                Kind::Separate,
                                1,
                                "Exec",
                                ""),
};

OptTable make_proxy_opt_table() {
    return OptTable(std::span<const OptTableInfo>(kProxyOptInfos))
        .set_tablegen_mode(false)
        .set_dash_dash_parsing(true)
        .set_dash_dash_as_single_pack(true);
}

TEST_SUITE(option_parse_view) {

TEST_CASE(main_option_table_basic) {
    auto table = make_main_opt_table();
    auto parsed = parse_all(
        table,
        split2vec("-p 1234 -s script::profile --dest=114514 -- /usr/bin/clang++ --version"));

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 5U);

    EXPECT_EQ(parsed.args[0].id, MAIN_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "-p");
    EXPECT_EQ(parsed.args[0].index, 0U);

    EXPECT_EQ(parsed.args[1].id, MAIN_OPT_INPUT);
    EXPECT_EQ(parsed.args[1].spelling, "1234");
    EXPECT_EQ(parsed.args[1].index, 1U);

    EXPECT_EQ(parsed.args[2].id, MAIN_OPT_SCRIPT);
    ASSERT_EQ(parsed.args[2].values.size(), 1U);
    EXPECT_EQ(parsed.args[2].values[0], "script::profile");
    EXPECT_EQ(parsed.args[2].spelling, "-s");
    EXPECT_EQ(parsed.args[2].index, 2U);

    EXPECT_EQ(parsed.args[3].id, MAIN_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[3].spelling, "--dest=114514");
    EXPECT_EQ(parsed.args[3].index, 4U);

    EXPECT_EQ(parsed.args[4].id, MAIN_OPT_INPUT);
    EXPECT_EQ(parsed.args[4].spelling, "--");
    ASSERT_EQ(parsed.args[4].values.size(), 2U);
    EXPECT_EQ(parsed.args[4].values[0], "/usr/bin/clang++");
    EXPECT_EQ(parsed.args[4].values[1], "--version");
    EXPECT_EQ(parsed.args[4].index, 5U);
}

TEST_CASE(alias_resolves_to_canonical) {
    auto table = make_main_opt_table();
    auto parsed = parse_all(table, split2vec("-h"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, MAIN_OPT_HELP);
    EXPECT_EQ(parsed.args[0].spelling, "-h");
    EXPECT_EQ(parsed.args[0].values.size(), 0U);
}

TEST_CASE(proxy_option_table_basic) {
    auto table = make_proxy_opt_table();

    auto parsed = parse_all(table, split2vec("-p 1234"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_PARENT_ID);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "1234");
    EXPECT_EQ(parsed.args[0].spelling, "-p");
    EXPECT_EQ(parsed.args[0].index, 0U);

    parsed = parse_all(table, split2vec("--exec /bin/ls"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_EXEC);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "/bin/ls");
    EXPECT_EQ(parsed.args[0].spelling, "--exec");
    EXPECT_EQ(parsed.args[0].index, 0U);

    parsed = parse_all(table, split2vec("-p 12 --exec /usr/bin/clang++ -- clang++ --version"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_PARENT_ID);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "12");
    EXPECT_EQ(parsed.args[0].index, 0U);

    EXPECT_EQ(parsed.args[1].id, PROXY_OPT_EXEC);
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "/usr/bin/clang++");
    EXPECT_EQ(parsed.args[1].index, 2U);

    EXPECT_EQ(parsed.args[2].id, PROXY_OPT_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "--");
    ASSERT_EQ(parsed.args[2].values.size(), 2U);
    EXPECT_EQ(parsed.args[2].values[0], "clang++");
    EXPECT_EQ(parsed.args[2].values[1], "--version");
    EXPECT_EQ(parsed.args[2].index, 4U);
}

TEST_CASE(proxy_missing_value_error) {
    auto table = make_proxy_opt_table();
    auto parsed = parse_all(table, split2vec("-p"));
    EXPECT_EQ(parsed.args.size(), 0U);
    ASSERT_EQ(parsed.errors.size(), 1U);
    EXPECT_TRUE(std::string_view(parsed.errors[0].message).contains("missing"));
}

};  // TEST_SUITE(option_parse_view)

enum GroupedOptionID {
    GROUPED_OPT_INVALID = 0,
    GROUPED_OPT_INPUT = 1,
    GROUPED_OPT_UNKNOWN = 2,
    GROUPED_OPT_A,
    GROUPED_OPT_B,
};

constexpr auto kGroupedOptInfos = std::array{
    OptTableInfo::input(GROUPED_OPT_INPUT),
    OptTableInfo::unknown(GROUPED_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_dash, "-a", GROUPED_OPT_A, Kind::Flag, 0, "", ""),
    OptTableInfo::unaliased_one(pfx_dash, "-b", GROUPED_OPT_B, Kind::Flag, 0, "", ""),
};

OptTable make_grouped_opt_table() {
    return OptTable(std::span<const OptTableInfo>(kGroupedOptInfos))
        .set_tablegen_mode(false)
        .set_grouped_short_options(true);
}

enum IgnoreCaseOptionID {
    IGNORE_CASE_OPT_INVALID = 0,
    IGNORE_CASE_OPT_INPUT = 1,
    IGNORE_CASE_OPT_UNKNOWN = 2,
    IGNORE_CASE_OPT_HELP,
};

constexpr auto kIgnoreCaseOptInfos = std::array{
    OptTableInfo::input(IGNORE_CASE_OPT_INPUT),
    OptTableInfo::unknown(IGNORE_CASE_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_double, "--help", IGNORE_CASE_OPT_HELP, Kind::Flag, 0, "", ""),
};

OptTable make_ignore_case_opt_table(bool ignore_case) {
    auto table = OptTable(std::span<const OptTableInfo>(kIgnoreCaseOptInfos));
    table.set_tablegen_mode(false);
    table.set_ignore_case(ignore_case);
    return table;
}

enum FilterOptionID {
    FILTER_OPT_INVALID = 0,
    FILTER_OPT_INPUT = 1,
    FILTER_OPT_UNKNOWN = 2,
    FILTER_OPT_PUBLIC,
    FILTER_OPT_HIDDEN,
    FILTER_OPT_FLAGGED,
};

constexpr std::uint32_t kInternalVisibility = 1U << 1;
constexpr std::uint32_t kExperimentalFlag = 1U << 6;

constexpr auto kFilterOptInfos = std::array{
    OptTableInfo::input(FILTER_OPT_INPUT),
    OptTableInfo::unknown(FILTER_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_double, "--public", FILTER_OPT_PUBLIC, Kind::Flag, 0, "", ""),
    OptTableInfo::unaliased_one(pfx_double,
                                "--hidden",
                                FILTER_OPT_HIDDEN,
                                Kind::Flag,
                                0,
                                "",
                                "",
                                0,
                                0,
                                kInternalVisibility),
    OptTableInfo::unaliased_one(pfx_double,
                                "--flagged",
                                FILTER_OPT_FLAGGED,
                                Kind::Flag,
                                0,
                                "",
                                "",
                                0,
                                kExperimentalFlag,
                                DefaultVis),
};

OptTable make_filter_opt_table() {
    return OptTable(std::span<const OptTableInfo>(kFilterOptInfos)).set_tablegen_mode(false);
}

enum KindsOptionID {
    KINDS_OPT_INVALID = 0,
    KINDS_OPT_INPUT = 1,
    KINDS_OPT_UNKNOWN = 2,
    KINDS_OPT_JOINED,
    KINDS_OPT_COMMA_JOINED,
    KINDS_OPT_MULTI_ARG,
    KINDS_OPT_JOINED_OR_SEPARATE,
    KINDS_OPT_JOINED_AND_SEPARATE,
    KINDS_OPT_REMAINING,
    KINDS_OPT_REMAINING_JOINED,
};

constexpr auto kKindsOptInfos = std::array{
    OptTableInfo::input(KINDS_OPT_INPUT),
    OptTableInfo::unknown(KINDS_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_dash, "-j", KINDS_OPT_JOINED, Kind::Joined, 1, "", ""),
    OptTableInfo::unaliased_one(pfx_double,
                                "--list",
                                KINDS_OPT_COMMA_JOINED,
                                Kind::CommaJoined,
                                1,
                                "",
                                ""),
    OptTableInfo::unaliased_one(pfx_double,
                                "--pair",
                                KINDS_OPT_MULTI_ARG,
                                Kind::MultiArg,
                                2,
                                "",
                                ""),
    OptTableInfo::unaliased_one(pfx_dash,
                                "-o",
                                KINDS_OPT_JOINED_OR_SEPARATE,
                                Kind::JoinedOrSeparate,
                                1,
                                "",
                                ""),
    OptTableInfo::unaliased_one(pfx_dash,
                                "-x",
                                KINDS_OPT_JOINED_AND_SEPARATE,
                                Kind::JoinedAndSeparate,
                                2,
                                "",
                                ""),
    OptTableInfo::unaliased_one(pfx_double,
                                "--rest",
                                KINDS_OPT_REMAINING,
                                Kind::RemainingArgs,
                                0,
                                "",
                                ""),
    OptTableInfo::unaliased_one(pfx_double,
                                "--tail",
                                KINDS_OPT_REMAINING_JOINED,
                                Kind::RemainingArgsJoined,
                                0,
                                "",
                                ""),
};

OptTable make_kinds_opt_table() {
    return OptTable(std::span<const OptTableInfo>(kKindsOptInfos)).set_tablegen_mode(false);
}

enum MatchOptionID {
    MATCH_OPT_INVALID = 0,
    MATCH_OPT_INPUT = 1,
    MATCH_OPT_UNKNOWN = 2,
    MATCH_OPT_GROUP,
    MATCH_OPT_MEMBER,
    MATCH_OPT_ALIAS_MEMBER,
    MATCH_OPT_JOINED,
    MATCH_OPT_OVERRIDE_FLAG,
};

constexpr auto kMatchOptInfos = std::array{
    OptTableInfo::input(MATCH_OPT_INPUT),
    OptTableInfo::unknown(MATCH_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_none, "group", MATCH_OPT_GROUP, Kind::Group, 0, "", ""),
    OptTableInfo::unaliased_one(pfx_dash,
                                "-m",
                                MATCH_OPT_MEMBER,
                                Kind::Flag,
                                0,
                                "",
                                "",
                                MATCH_OPT_GROUP),
    OptTableInfo::unaliased_one(pfx_dash, "-am", MATCH_OPT_ALIAS_MEMBER, Kind::Flag, 0, "", "")
        .alias_of(MATCH_OPT_MEMBER),
    OptTableInfo::unaliased_one(pfx_dash, "-j", MATCH_OPT_JOINED, Kind::Joined, 1, "", ""),
    OptTableInfo::unaliased_one(pfx_dash,
                                "-r",
                                MATCH_OPT_OVERRIDE_FLAG,
                                Kind::Flag,
                                0,
                                "",
                                "",
                                0,
                                RenderJoined),
};

OptTable make_match_opt_table() {
    return OptTable(std::span<const OptTableInfo>(kMatchOptInfos)).set_tablegen_mode(false);
}

enum AliasOptionID {
    ALIAS_OPT_INVALID = 0,
    ALIAS_OPT_INPUT = 1,
    ALIAS_OPT_UNKNOWN = 2,
    ALIAS_OPT_TRAP_EQ,
    ALIAS_OPT_TRAP_DEFAULTS,
    ALIAS_OPT_EMIT_EQ,
    ALIAS_OPT_EMIT_LLVM,
};

constexpr auto kAliasOptInfos = std::array{
    OptTableInfo::input(ALIAS_OPT_INPUT),
    OptTableInfo::unknown(ALIAS_OPT_UNKNOWN),
    OptTableInfo::unaliased_one(pfx_double,
                                "--trap=",
                                ALIAS_OPT_TRAP_EQ,
                                Kind::CommaJoined,
                                1,
                                "",
                                ""),
    OptTableInfo::unaliased_one(pfx_double,
                                "--trap-defaults",
                                ALIAS_OPT_TRAP_DEFAULTS,
                                Kind::Flag,
                                0,
                                "",
                                "")
        .alias_of(ALIAS_OPT_TRAP_EQ, "all\0undefined\0"),
    OptTableInfo::unaliased_one(pfx_double, "--emit=", ALIAS_OPT_EMIT_EQ, Kind::Joined, 1, "", ""),
    OptTableInfo::unaliased_one(pfx_double,
                                "--emit-llvm",
                                ALIAS_OPT_EMIT_LLVM,
                                Kind::Flag,
                                0,
                                "",
                                "")
        .alias_of(ALIAS_OPT_EMIT_EQ),
};

OptTable make_alias_opt_table() {
    return OptTable(std::span<const OptTableInfo>(kAliasOptInfos)).set_tablegen_mode(false);
}

TEST_SUITE(option_extended_coverage) {

TEST_CASE(ignore_case_controls_matching) {
    auto strict_table = make_ignore_case_opt_table(false);
    auto parsed = parse_all(strict_table, split2vec("--HELP"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, IGNORE_CASE_OPT_UNKNOWN);

    auto ignore_case_table = make_ignore_case_opt_table(true);
    parsed = parse_all(ignore_case_table, split2vec("--HELP"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, IGNORE_CASE_OPT_HELP);
}

TEST_CASE(grouped_short_option_parsing) {
    auto table = make_grouped_opt_table();

    auto parsed = parse_all(table, split2vec("-ab"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);
    EXPECT_EQ(parsed.args[0].id, GROUPED_OPT_A);
    EXPECT_EQ(parsed.args[1].id, GROUPED_OPT_B);
}

TEST_CASE(grouped_unknown_splits) {
    auto table = make_grouped_opt_table();

    auto parsed = parse_all(table, split2vec("-zx"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);
    EXPECT_EQ(parsed.args[0].id, GROUPED_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[1].id, GROUPED_OPT_UNKNOWN);
}

TEST_CASE(grouped_equals_form_is_unknown) {
    auto table = make_grouped_opt_table();

    auto parsed = parse_all(table, split2vec("-a=1"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, GROUPED_OPT_UNKNOWN);
}

TEST_CASE(visibility_filter_affects_matching) {
    auto table = make_filter_opt_table();

    auto argv = split2vec("--hidden");
    ParseCapture default_vis;
    for(auto& result: table.parse(argv, OptFilter(DefaultVis))) {
        if(result.has_value())
            default_vis.args.push_back(*result);
        else
            default_vis.errors.push_back(result.error());
    }
    EXPECT_TRUE(default_vis.errors.empty());
    ASSERT_EQ(default_vis.args.size(), 1U);
    EXPECT_EQ(default_vis.args[0].id, FILTER_OPT_UNKNOWN);

    argv = split2vec("--hidden");
    ParseCapture capture;
    for(auto& result: table.parse(argv, OptFilter(kInternalVisibility))) {
        if(result.has_value())
            capture.args.push_back(*result);
        else
            capture.errors.push_back(result.error());
    }
    EXPECT_TRUE(capture.errors.empty());
    ASSERT_EQ(capture.args.size(), 1U);
    EXPECT_EQ(capture.args[0].id, FILTER_OPT_HIDDEN);
}

TEST_CASE(flag_filter_affects_matching) {
    auto table = make_filter_opt_table();

    auto argv = split2vec("--flagged");
    ParseCapture include_capture;
    for(auto& result: table.parse(argv, {~0U, kExperimentalFlag, 0})) {
        if(result.has_value())
            include_capture.args.push_back(*result);
        else
            include_capture.errors.push_back(result.error());
    }
    EXPECT_TRUE(include_capture.errors.empty());
    ASSERT_EQ(include_capture.args.size(), 1U);
    EXPECT_EQ(include_capture.args[0].id, FILTER_OPT_FLAGGED);

    argv = split2vec("--flagged");
    ParseCapture exclude_capture;
    for(auto& result: table.parse(argv, {~0U, 0, kExperimentalFlag})) {
        if(result.has_value())
            exclude_capture.args.push_back(*result);
        else
            exclude_capture.errors.push_back(result.error());
    }
    EXPECT_TRUE(exclude_capture.errors.empty());
    ASSERT_EQ(exclude_capture.args.size(), 1U);
    EXPECT_EQ(exclude_capture.args[0].id, FILTER_OPT_UNKNOWN);
}

TEST_CASE(option_kinds_parse_correctly) {
    auto table = make_kinds_opt_table();

    auto parsed = parse_all(table, split2vec("-jabc"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "abc");

    parsed = parse_all(table, split2vec("--list=a,,b,c"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_COMMA_JOINED);
    ASSERT_EQ(parsed.args[0].values.size(), 3U);
    EXPECT_EQ(parsed.args[0].values[0], "=a");
    EXPECT_EQ(parsed.args[0].values[1], "b");
    EXPECT_EQ(parsed.args[0].values[2], "c");

    parsed = parse_all(table, split2vec("--pair left right"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_MULTI_ARG);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "left");
    EXPECT_EQ(parsed.args[0].values[1], "right");

    parsed = parse_all(table, split2vec("-o2"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED_OR_SEPARATE);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "2");

    parsed = parse_all(table, split2vec("-o 3"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED_OR_SEPARATE);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "3");

    parsed = parse_all(table, split2vec("-x4 tail"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED_AND_SEPARATE);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "4");
    EXPECT_EQ(parsed.args[0].values[1], "tail");

    parsed = parse_all(table, split2vec("--rest one two"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_REMAINING);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "one");
    EXPECT_EQ(parsed.args[0].values[1], "two");

    parsed = parse_all(table, split2vec("--tailz one two"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_REMAINING_JOINED);
    ASSERT_EQ(parsed.args[0].values.size(), 3U);
    EXPECT_EQ(parsed.args[0].values[0], "z");
    EXPECT_EQ(parsed.args[0].values[1], "one");
    EXPECT_EQ(parsed.args[0].values[2], "two");
}

TEST_CASE(option_matches_and_render_style) {
    auto table = make_match_opt_table();

    const auto member = table.option(MATCH_OPT_MEMBER);
    const auto alias = table.option(MATCH_OPT_ALIAS_MEMBER);
    EXPECT_TRUE(member.matches(MATCH_OPT_GROUP));
    EXPECT_TRUE(alias.matches(MATCH_OPT_GROUP));
    EXPECT_EQ(alias.unaliased_option().id(), MATCH_OPT_MEMBER);

    EXPECT_EQ(table.option(MATCH_OPT_JOINED).render_style(), RenderStyle::Joined);
    EXPECT_EQ(table.option(MATCH_OPT_MEMBER).render_style(), RenderStyle::Separate);
    EXPECT_EQ(table.option(MATCH_OPT_OVERRIDE_FLAG).render_style(), RenderStyle::Joined);
}

TEST_CASE(flag_aliases_merge_values) {
    auto table = make_alias_opt_table();
    auto parsed = parse_all(table, split2vec("--trap-defaults --emit-llvm"));

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);

    EXPECT_EQ(parsed.args[0].id, ALIAS_OPT_TRAP_EQ);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "all");
    EXPECT_EQ(parsed.args[0].values[1], "undefined");

    EXPECT_EQ(parsed.args[1].id, ALIAS_OPT_EMIT_EQ);
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "");
}

TEST_CASE(find_option_basic) {
    auto table = make_main_opt_table();

    EXPECT_EQ(table.find_option("--help"), MAIN_OPT_HELP);
    EXPECT_EQ(table.find_option("-h"), MAIN_OPT_HELP);
    EXPECT_EQ(table.find_option("-s"), MAIN_OPT_SCRIPT);
    EXPECT_EQ(table.find_option("--nonexistent"), MAIN_OPT_UNKNOWN);
}

};  // TEST_SUITE(option_extended_coverage)

}  // namespace
}  // namespace kota::option
