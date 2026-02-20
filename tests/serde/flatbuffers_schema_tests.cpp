#if __has_include(<flatbuffers/flatbuffer_builder.h>)

#include <map>
#include <string>
#include <vector>

#include "zest/zest.h"
#include "serde/flatbuffers/schema/schema.h"

namespace serde::testing {

namespace {

struct point2d {
    std::int32_t x;
    std::int32_t y;
};

struct payload {
    point2d point;
    std::string name;
    std::vector<std::int32_t> values;
    std::map<std::string, std::int32_t> attrs;
};

TEST_SUITE(serde_flatbuffers_schema) {

TEST_CASE(trivial_struct_maps_to_schema_struct) {
    ASSERT_TRUE((serde::flatbuffers::schema::is_schema_struct_v<point2d>));
    ASSERT_FALSE((serde::flatbuffers::schema::is_schema_struct_v<payload>));
}

TEST_CASE(map_field_emits_binary_search_entry_table) {
    const auto schema = serde::flatbuffers::schema::render<payload>();
    const auto point_name = serde::flatbuffers::schema::type_identifier<point2d>();
    const auto payload_name = serde::flatbuffers::schema::type_identifier<payload>();
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

    auto entries = serde::flatbuffers::schema::to_sorted_entries(input);
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].key, "alpha");
    EXPECT_EQ(entries[1].key, "mid");
    EXPECT_EQ(entries[2].key, "zeta");

    auto found = serde::flatbuffers::schema::bsearch_entry(entries, std::string("mid"));
    ASSERT_TRUE(found != nullptr);
    EXPECT_EQ(found->value, 2);

    auto missing = serde::flatbuffers::schema::bsearch_entry(entries, std::string("none"));
    EXPECT_TRUE(missing == nullptr);
}

};  // TEST_SUITE(serde_flatbuffers_schema)

}  // namespace

}  // namespace serde::testing

#endif
