#if __has_include(<flatbuffers/flatbuffer_builder.h>)

#include <map>
#include <string>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/serde/flatbuffers/schema.h"
#include "eventide/serde/schema/codegen/fbs.h"
#include "eventide/serde/schema/virtual_schema.h"

namespace eventide::serde {

namespace {

struct point2d {
    std::int32_t x;
    std::int32_t y;
};

enum class color : std::int8_t { red = 0, green = 1, blue = 2 };

struct payload {
    point2d point;
    std::string name;
    std::vector<std::int32_t> values;
    std::map<std::string, std::int32_t> attrs;
};

struct colored_payload {
    color col;
    std::string label;
    std::vector<point2d> points;
};

TEST_SUITE(serde_flatbuffers_schema) {

TEST_CASE(trivial_struct_maps_to_schema_struct) {
    ASSERT_TRUE((flatbuffers::is_schema_struct_v<point2d>));
    ASSERT_FALSE((flatbuffers::is_schema_struct_v<payload>));
}

TEST_CASE(map_field_emits_binary_search_entry_table) {
    const auto schema = schema::codegen::fbs::render(schema::type_info_of<payload>());
    const auto point_name = schema::codegen::fbs::normalize_identifier(refl::type_name<point2d>());
    const auto payload_name =
        schema::codegen::fbs::normalize_identifier(refl::type_name<payload>());
    const auto map_entry_name = payload_name + "_attrsEntry";

    EXPECT_NE(schema.find("struct " + point_name), std::string::npos);
    EXPECT_NE(schema.find("table " + payload_name), std::string::npos);
    EXPECT_NE(schema.find("table " + map_entry_name), std::string::npos);
    EXPECT_NE(schema.find("key:string (key);"), std::string::npos);
    EXPECT_NE(schema.find("attrs:[" + map_entry_name + "];"), std::string::npos);
}

TEST_CASE(sorted_entries_support_binary_search_lookup) {
    const std::map<std::string, int> input{
        {"zeta",  3},
        {"alpha", 1},
        {"mid",   2},
    };

    auto entries = flatbuffers::to_sorted_entries(input);
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].key, "alpha");
    EXPECT_EQ(entries[1].key, "mid");
    EXPECT_EQ(entries[2].key, "zeta");

    auto found = flatbuffers::bsearch_entry(entries, std::string("mid"));
    ASSERT_TRUE(found != nullptr);
    EXPECT_EQ(found->value, 2);

    auto missing = flatbuffers::bsearch_entry(entries, std::string("none"));
    EXPECT_TRUE(missing == nullptr);
}

TEST_CASE(render_simple_struct) {
    const auto schema = schema::codegen::fbs::render(schema::type_info_of<point2d>());
    EXPECT_NE(schema.find("struct point2d"), std::string::npos);
    EXPECT_NE(schema.find("root_type point2d;"), std::string::npos);
}

TEST_CASE(render_with_enum) {
    const auto schema = schema::codegen::fbs::render(schema::type_info_of<colored_payload>());
    EXPECT_NE(schema.find("enum color:byte"), std::string::npos);
    EXPECT_NE(schema.find("table colored_payload"), std::string::npos);
    EXPECT_NE(schema.find("col:color"), std::string::npos);
}

};  // TEST_SUITE(serde_flatbuffers_schema)

}  // namespace

}  // namespace eventide::serde

#endif
