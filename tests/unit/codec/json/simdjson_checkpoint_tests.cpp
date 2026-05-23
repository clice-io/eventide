/// Tests that try_read correctly saves and restores all json_iterator state
/// on failure. Uses Source::json_iter() to verify internal simdjson state.

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"

namespace kota::codec {

namespace {

using json::from_json;
using json::Reader;
using json::Source;

struct SimpleA {
    int x = 0;
    int y = 0;
    std::string name;
};

struct SimpleB {
    std::string value;
};

using SimpleVariant = std::variant<SimpleA, SimpleB>;

struct HasRequired {
    int id = 0;
    std::string label;
    std::string data;
};

struct HasOptional {
    std::optional<int> id;
    std::string data;
};

struct Nested {
    std::string a;
    std::string b;
    std::string c;
};

struct Shallow {
    std::string a;
};

using NestedVariant = std::variant<Nested, Shallow>;

struct Wrapper {
    std::vector<SimpleVariant> items;
};

TEST_SUITE(simdjson_checkpoint_restore) {

TEST_CASE(string_buffer_reclaimed) {
    std::string big(16384, 'X');
    std::string input = R"({"value":")" + big + R"("})";

    json::padded_string padded{std::string_view{input}};
    json::ondemand::Parser parser;
    json::ondemand::Document doc;
    ASSERT_EQ(parser.iterate(padded).get(doc), json::success);
    auto r = Reader{doc, padded.data(), padded.size()};
    auto& ji = r.src.json_iter();
    auto* buf_before = ji.string_buf_loc();

    bool ok = r.try_read([&](Reader& sub) -> bool {
        SimpleA a;
        return decode_value<default_config<>>(sub, a);
    });
    EXPECT_FALSE(ok);
    EXPECT_EQ(ji.string_buf_loc(), buf_before);
}

TEST_CASE(depth_restored) {
    auto input = R"({"value":"test"})";
    json::padded_string padded{std::string_view{input}};
    json::ondemand::Parser parser;
    json::ondemand::Document doc;
    ASSERT_EQ(parser.iterate(padded).get(doc), json::success);
    auto r = Reader{doc, padded.data(), padded.size()};
    auto& ji = r.src.json_iter();
    auto depth_before = ji.depth();

    bool ok = r.try_read([&](Reader& sub) -> bool {
        SimpleA a;
        return decode_value<default_config<>>(sub, a);
    });
    EXPECT_FALSE(ok);
    EXPECT_EQ(ji.depth(), depth_before);
}

TEST_CASE(token_position_restored) {
    auto input = R"({"value":"test"})";
    json::padded_string padded{std::string_view{input}};
    json::ondemand::Parser parser;
    json::ondemand::Document doc;
    ASSERT_EQ(parser.iterate(padded).get(doc), json::success);
    auto r = Reader{doc, padded.data(), padded.size()};
    auto& ji = r.src.json_iter();
    auto pos_before = ji.position();

    bool ok = r.try_read([&](Reader& sub) -> bool {
        SimpleA a;
        return decode_value<default_config<>>(sub, a);
    });
    EXPECT_FALSE(ok);
    EXPECT_EQ(ji.position(), pos_before);
}

TEST_CASE(all_state_with_large_string) {
    std::string big(32768, 'Z');
    std::string input = R"({"data":")" + big + R"("})";

    json::padded_string padded{std::string_view{input}};
    json::ondemand::Parser parser;
    json::ondemand::Document doc;
    ASSERT_EQ(parser.iterate(padded).get(doc), json::success);
    auto r = Reader{doc, padded.data(), padded.size()};
    auto& ji = r.src.json_iter();

    auto pos_before = ji.position();
    auto* buf_before = ji.string_buf_loc();
    auto depth_before = ji.depth();

    bool ok = r.try_read([&](Reader& sub) -> bool {
        HasRequired h;
        return decode_value<default_config<>>(sub, h);
    });
    EXPECT_FALSE(ok);

    EXPECT_EQ(ji.position(), pos_before);
    EXPECT_EQ(ji.string_buf_loc(), buf_before);
    EXPECT_EQ(ji.depth(), depth_before);
}

TEST_CASE(success_advances_state) {
    auto input = R"({"value":"hello"})";
    json::padded_string padded{std::string_view{input}};
    json::ondemand::Parser parser;
    json::ondemand::Document doc;
    ASSERT_EQ(parser.iterate(padded).get(doc), json::success);
    auto r = Reader{doc, padded.data(), padded.size()};
    auto& ji = r.src.json_iter();

    auto pos_before = ji.position();
    auto* buf_before = ji.string_buf_loc();

    bool ok = r.try_read([&](Reader& sub) -> bool {
        SimpleB b;
        return decode_value<default_config<>>(sub, b);
    });
    EXPECT_TRUE(ok);
    EXPECT_NE(ji.position(), pos_before);
    EXPECT_NE(ji.string_buf_loc(), buf_before);
}

TEST_CASE(multiple_failures_no_drift) {
    auto input = R"({"value":"hello"})";
    json::padded_string padded{std::string_view{input}};
    json::ondemand::Parser parser;
    json::ondemand::Document doc;
    ASSERT_EQ(parser.iterate(padded).get(doc), json::success);
    auto r = Reader{doc, padded.data(), padded.size()};
    auto& ji = r.src.json_iter();

    auto pos_original = ji.position();
    auto* buf_original = ji.string_buf_loc();
    auto depth_original = ji.depth();

    for(int i = 0; i < 5; ++i) {
        bool ok = r.try_read([&](Reader& sub) -> bool {
            SimpleA a;
            return decode_value<default_config<>>(sub, a);
        });
        EXPECT_FALSE(ok);
    }

    EXPECT_EQ(ji.position(), pos_original);
    EXPECT_EQ(ji.string_buf_loc(), buf_original);
    EXPECT_EQ(ji.depth(), depth_original);
}

TEST_CASE(variant_fallback_reclaims) {
    // variant<Nested, Shallow>: Nested fails (missing b, c), Shallow succeeds.
    // String buffer consumed during Nested attempt must be reclaimed.
    std::string big(8192, 'D');
    std::string input = R"({"a":")" + big + R"("})";

    NestedVariant out;
    auto result = from_json<>(input, out);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(out.index(), 1U);
    EXPECT_EQ(std::get<Shallow>(out).a.size(), 8192U);
}

TEST_CASE(variant_fallback_reclaims_256KB) {
    std::string big(256 * 1024, 'E');
    std::string input = R"({"a":")" + big + R"("})";

    NestedVariant out;
    auto result = from_json<>(input, out);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(out.index(), 1U);
    EXPECT_EQ(std::get<Shallow>(out).a.size(), 256U * 1024U);
}

TEST_CASE(variant_first_alternative_succeeds) {
    // Sanity: when the first alternative matches, no checkpoint restore needed.
    std::string s1(4096, 'A');
    std::string s2(4096, 'B');
    std::string s3(4096, 'C');
    std::string input = R"({"a":")" + s1 + R"(","b":")" + s2 + R"(","c":")" + s3 + R"("})";

    NestedVariant out;
    auto result = from_json<>(input, out);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(out.index(), 0U);
    auto& n = std::get<Nested>(out);
    EXPECT_EQ(n.a.size(), 4096U);
    EXPECT_EQ(n.b.size(), 4096U);
    EXPECT_EQ(n.c.size(), 4096U);
}

TEST_CASE(value_level_variant) {
    // Variant inside an array: try_read operates on a Value, not Document.
    auto input = R"({"items":[{"value":"world"}]})";
    Wrapper w;
    auto result = from_json<>(input, w);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(w.items.size(), 1U);
    ASSERT_EQ(w.items[0].index(), 1U);
    EXPECT_EQ(std::get<SimpleB>(w.items[0]).value, "world");
}

};  // TEST_SUITE(simdjson_checkpoint_restore)

}  // namespace

}  // namespace kota::codec
