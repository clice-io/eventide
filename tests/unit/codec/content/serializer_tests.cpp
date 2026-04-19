#include <optional>
#include <utility>

#include "kota/zest/zest.h"
#include "kota/codec/content.h"

namespace kota::codec {

namespace {

TEST_SUITE(serde_content_serializer) {

TEST_CASE(dom_value_requires_closed_containers) {
    content::Serializer<> serializer;

    auto seq = serializer.serialize_seq(std::nullopt);
    ASSERT_TRUE(seq.has_value());
    ASSERT_TRUE(seq->serialize_element(1).has_value());

    auto incomplete = serializer.dom_value();
    ASSERT_FALSE(incomplete.has_value());
    EXPECT_EQ(incomplete.error(), content::error_kind::invalid_state);

    ASSERT_TRUE(seq->end().has_value());

    auto complete = serializer.dom_value();
    ASSERT_TRUE(complete.has_value());
    ASSERT_TRUE(complete->is_array());
    EXPECT_EQ(complete->as_array().size(), std::size_t(1));
    EXPECT_EQ(complete->as_array()[0].as_int(), 1);
}

TEST_CASE(take_dom_value_moves_root_out) {
    content::Serializer<> serializer;

    auto map = serializer.serialize_map(std::nullopt);
    ASSERT_TRUE(map.has_value());
    ASSERT_TRUE(map->serialize_entry("a", 1).has_value());
    ASSERT_TRUE(map->serialize_entry("b", 2).has_value());
    ASSERT_TRUE(map->end().has_value());

    auto taken = serializer.take_dom_value();
    ASSERT_TRUE(taken.has_value());
    ASSERT_TRUE(taken->is_object());

    const auto& object = taken->as_object();
    EXPECT_EQ(object.size(), std::size_t(2));
    EXPECT_EQ(object.at("a").as_int(), 1);
    EXPECT_EQ(object.at("b").as_int(), 2);
}

TEST_CASE(append_dom_value_inlines_foreign_subtree) {
    content::Serializer<> serializer;

    content::Object subtree;
    subtree.insert("k", content::Value(std::int64_t(9)));

    auto seq = serializer.serialize_seq(std::nullopt);
    ASSERT_TRUE(seq.has_value());
    ASSERT_TRUE(serializer.append_dom_value(std::move(subtree)).has_value());
    ASSERT_TRUE(seq->end().has_value());

    auto dom = serializer.dom_value();
    ASSERT_TRUE(dom.has_value());
    ASSERT_TRUE(dom->is_array());
    const auto& array = dom->as_array();
    ASSERT_EQ(array.size(), std::size_t(1));
    ASSERT_TRUE(array[0].is_object());
    EXPECT_EQ(array[0].as_object().at("k").as_int(), 9);
}

};  // TEST_SUITE(serde_content_serializer)

}  // namespace

}  // namespace kota::codec
