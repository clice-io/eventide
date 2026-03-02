#include "eventide/deco/serialize.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "eventide/deco/macro.h"
#include "eventide/deco/runtime.h"
#include "eventide/zest/zest.h"

namespace {

constexpr deco::decl::Category primary_category = {
    .exclusive = false,
    .required = false,
    .name = "primary",
    .description = "primary serialization group",
};

constexpr deco::decl::Category secondary_category = {
    .exclusive = false,
    .required = false,
    .name = "secondary",
    .description = "secondary serialization group",
};

constexpr deco::decl::Category trailing_category = {
    .exclusive = false,
    .required = false,
    .name = "trailing",
    .description = "trailing args group",
};

struct SerializeOpt {
    DecoFlag(names = {"-v", "--verbose"}; required = false; category = primary_category;)
    verbose;

    DecoFlagN(names = {"-n"}, required = false, category = primary_category)
    repeat;

    DecoKV(names = {"--count"}; required = false; category = primary_category;)
    <int> count;

    DecoKVStyled(deco::decl::KVStyle::Joined, names = {"--joined"}; required = false;
                 category = primary_category;)
    <int> joined_only;

    DecoKVStyled(static_cast<char>(deco::decl::KVStyle::Joined | deco::decl::KVStyle::Separate),
                 names = {"--split="};
                 required = false;
                 category = secondary_category;)
    <int> split_by_name;

    DecoKV(required = false; category = secondary_category;)
    <int> auto_name_value;

    DecoComma(names = {"--tags"}; required = false; category = secondary_category;)
    <std::vector<std::string>> tags;

    DecoMulti(2, {
        names = {"--pair"};
        required = false;
        category = secondary_category;
    })
    <std::vector<std::string>> pair;

    DecoPack(required = false; category = trailing_category;)
    <std::vector<std::string>> trailing;

    DecoInput(required = false; category = primary_category;)
    <std::string> input;
};

struct TrailingFirstOpt {
    DecoPack(required = false; category = trailing_category;)
    <std::vector<std::string>> trailing;

    DecoFlag(names = {"--dry-run"}; required = false; category = primary_category;)
    dry_run;

    DecoInput(required = false; category = primary_category;)
    <std::string> input;
};

SerializeOpt make_full_opt() {
    SerializeOpt opt{};
    opt.verbose = true;
    opt.repeat = static_cast<std::uint32_t>(2);
    opt.count = 7;
    opt.joined_only = 9;
    opt.split_by_name = 3;
    opt.auto_name_value = 11;
    opt.tags = std::vector<std::string>{"a", "b"};
    opt.pair = std::vector<std::string>{"left", "right"};
    opt.trailing = std::vector<std::string>{"tail1", "tail2"};
    opt.input = std::string("main.cc");
    return opt;
}

}  // namespace

TEST_SUITE(deco_serialize) {

TEST_CASE(optional_and_empty_values_are_not_generated) {
    SerializeOpt opt{};
    auto argv = deco::ser::to_argv(opt);
    EXPECT_TRUE(argv.empty());

    opt.verbose = false;
    opt.repeat = static_cast<std::uint32_t>(0);
    opt.tags = std::vector<std::string>{};
    opt.pair = std::vector<std::string>{};
    auto argv2 = deco::ser::to_argv(opt);
    EXPECT_TRUE(argv2.empty());
}

TEST_CASE(serializes_all_option_kinds_with_stable_order_and_roundtrip) {
    auto opt = make_full_opt();
    auto argv = deco::ser::to_argv(opt);

    const std::vector<std::string> expected = {"-v",
                                               "-n",
                                               "-n",
                                               "--count",
                                               "7",
                                               "--joined9",
                                               "--split=3",
                                               "--auto-name-value",
                                               "11",
                                               "--tags,a,b",
                                               "--pair",
                                               "left",
                                               "right",
                                               "main.cc",
                                               "--",
                                               "tail1",
                                               "tail2"};
    EXPECT_TRUE(argv == expected);

    auto parsed = deco::cli::parse<SerializeOpt>(argv);
    EXPECT_TRUE(parsed.has_value());
    if(!parsed.has_value()) {
        return;
    }
    const auto& value = parsed->options;
    EXPECT_TRUE(value.verbose.value.has_value() && *value.verbose.value);
    EXPECT_TRUE(value.repeat.value.has_value() && *value.repeat.value == 2u);
    EXPECT_TRUE(value.count.value.has_value() && *value.count.value == 7);
    EXPECT_TRUE(value.joined_only.value.has_value() && *value.joined_only.value == 9);
    EXPECT_TRUE(value.split_by_name.value.has_value() && *value.split_by_name.value == 3);
    EXPECT_TRUE(value.auto_name_value.value.has_value() && *value.auto_name_value.value == 11);
    EXPECT_TRUE(value.tags.value.has_value() &&
                *value.tags.value == std::vector<std::string>{"a", "b"});
    EXPECT_TRUE(value.pair.value.has_value() &&
                *value.pair.value == std::vector<std::string>{"left", "right"});
    EXPECT_TRUE(value.input.value.has_value() && *value.input.value == "main.cc");
    EXPECT_TRUE(value.trailing.value.has_value() &&
                *value.trailing.value == std::vector<std::string>{"tail1", "tail2"});
}

TEST_CASE(category_filter_generates_only_selected_groups) {
    auto opt = make_full_opt();

    auto primary_only = deco::ser::to_argv(opt, primary_category);
    const std::vector<std::string> expected_primary =
        {"-v", "-n", "-n", "--count", "7", "--joined9", "main.cc"};
    EXPECT_TRUE(primary_only == expected_primary);

    const deco::decl::Category* selected[] = {&secondary_category, &trailing_category};
    auto secondary_and_trailing =
        deco::ser::to_argv(opt, std::span<const deco::decl::Category* const>(selected));
    const std::vector<std::string> expected_secondary = {"--split=3",
                                                         "--auto-name-value",
                                                         "11",
                                                         "--tags,a,b",
                                                         "--pair",
                                                         "left",
                                                         "right",
                                                         "--",
                                                         "tail1",
                                                         "tail2"};
    EXPECT_TRUE(secondary_and_trailing == expected_secondary);
}

TEST_CASE(trailing_pack_is_emitted_after_non_trailing_arguments) {
    TrailingFirstOpt opt{};
    opt.trailing = std::vector<std::string>{"a", "b"};
    opt.dry_run = true;
    opt.input = std::string("front");

    auto argv = deco::ser::to_argv(opt);
    const std::vector<std::string> expected = {"--dry-run", "front", "--", "a", "b"};
    EXPECT_TRUE(argv == expected);
}

};  // TEST_SUITE(deco_serialize)
