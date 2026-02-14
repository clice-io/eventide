#include <array>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "zest/zest.h"
#include "serde/json.h"

namespace eventide::serde {

namespace {

struct person_yy {
    int id = 0;
    std::string name;
    std::vector<int> scores;
};

}  // namespace

template <class Serializer>
struct serialize_traits<Serializer, person_yy> {
    static void serialize(Serializer& serializer, const person_yy& value) {
        auto object = serializer.serialize_struct("person_yy", 3);
        object.serialize_field("id", value.id);
        object.serialize_field("name", value.name);
        object.serialize_field("scores", value.scores);
        object.end();
    }
};

template <>
struct deserialize_traits<eventide::serde::json::yy::Deserializer, person_yy> {
    static deserialize_result_t<person_yy, eventide::serde::json::yy::Deserializer>
        deserialize(eventide::serde::json::yy::Deserializer& deserializer) {
        using yy_deserializer_t = eventide::serde::json::yy::Deserializer;
        using error_t = deserialize_error_t<yy_deserializer_t>;

        struct person_visitor {
            using value_type = person_yy;

            std::string_view expecting() const {
                return "person_yy object";
            }

            deserialize_result_t<person_yy, yy_deserializer_t>
                visit_map(yy_deserializer_t::MapAccess& map) {
                person_yy out{};
                bool has_id = false;
                bool has_name = false;
                bool has_scores = false;

                while(true) {
                    auto key = map.template next_key<std::string_view>();
                    if(!key) {
                        return std::unexpected(key.error());
                    }
                    if(!*key) {
                        break;
                    }

                    if(**key == "id") {
                        auto parsed = map.template next_value<int>();
                        if(!parsed) {
                            return std::unexpected(parsed.error());
                        }
                        out.id = *parsed;
                        has_id = true;
                    } else if(**key == "name") {
                        auto parsed = map.template next_value<std::string>();
                        if(!parsed) {
                            return std::unexpected(parsed.error());
                        }
                        out.name = std::move(*parsed);
                        has_name = true;
                    } else if(**key == "scores") {
                        auto parsed = map.template next_value<std::vector<int>>();
                        if(!parsed) {
                            return std::unexpected(parsed.error());
                        }
                        out.scores = std::move(*parsed);
                        has_scores = true;
                    } else {
                        auto ignored = map.template next_value<eventide::serde::ignored_any>();
                        if(!ignored) {
                            return std::unexpected(ignored.error());
                        }
                    }
                }

                if(!has_id || !has_name || !has_scores) {
                    return std::unexpected(detail::error_traits<error_t>::invalid_argument());
                }

                return out;
            }
        } visitor{};

        constexpr std::array<std::string_view, 3> fields{"id", "name", "scores"};
        return deserializer.deserialize_struct("person_yy",
                                               std::span<const std::string_view>(fields),
                                               visitor);
    }
};

namespace {

using serializer_t = eventide::serde::json::yy::Serializer;
using deserializer_t = eventide::serde::json::yy::Deserializer;

struct any_bool_visitor {
    using value_type = bool;

    std::string_view expecting() const {
        return "boolean";
    }

    deserializer_t::result_t<bool> visit_bool(bool value) {
        return value;
    }

    deserializer_t::result_t<bool> visit_str(std::string_view) {
        return false;
    }
};

struct seq_sum_visitor {
    using value_type = int;

    std::string_view expecting() const {
        return "sequence of integers";
    }

    deserializer_t::result_t<int> visit_seq(deserializer_t::SeqAccess& access) {
        int sum = 0;
        while(true) {
            auto element = access.next_element<int>();
            if(!element) {
                return std::unexpected(element.error());
            }
            if(!*element) {
                break;
            }
            sum += **element;
        }
        return sum;
    }
};

struct map_sum_visitor {
    using value_type = int;

    std::string_view expecting() const {
        return "map of integer values";
    }

    deserializer_t::result_t<int> visit_map(deserializer_t::MapAccess& access) {
        int sum = 0;
        while(true) {
            auto key = access.next_key<std::string_view>();
            if(!key) {
                return std::unexpected(key.error());
            }
            if(!*key) {
                break;
            }

            auto value = access.next_value<int>();
            if(!value) {
                return std::unexpected(value.error());
            }
            sum += *value;
        }
        return sum;
    }
};

TEST_SUITE(serde_yyjson) {

TEST_CASE(serialize_vector) {
    std::vector<int> value{1, 2, 3, 5, 8};

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto dom = serializer.dom();
    ASSERT_TRUE(dom.has_value());
    auto out = dom->str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"([1,2,3,5,8])");
}

TEST_CASE(deserialize_vector) {
    std::string json = R"([13,21,34])";

    auto dom = eventide::serde::json::yy::parse(json);
    ASSERT_TRUE(dom.has_value());

    deserializer_t deserializer(std::move(*dom));

    auto value = eventide::serde::deserialize<std::vector<int>>(deserializer);
    ASSERT_TRUE(value.has_value());

    ASSERT_EQ(value->size(), 3U);
    EXPECT_EQ(value->at(0), 13);
    EXPECT_EQ(value->at(1), 21);
    EXPECT_EQ(value->at(2), 34);
}

TEST_CASE(serialize_map_vector) {
    std::map<int, std::vector<int>> value{
        {1, {2, 3}},
        {4, {5}   },
    };

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto dom = serializer.dom();
    ASSERT_TRUE(dom.has_value());
    auto out = dom->str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"1":[2,3],"4":[5]})");
}

TEST_CASE(deserialize_map_vector) {
    std::string json = R"({"1":[2,3],"4":[5]})";

    auto dom = eventide::serde::json::yy::parse(json);
    ASSERT_TRUE(dom.has_value());

    deserializer_t deserializer(std::move(*dom));

    auto value = eventide::serde::deserialize<std::map<int, std::vector<int>>>(deserializer);
    ASSERT_TRUE(value.has_value());

    EXPECT_EQ(value->size(), 2U);
    EXPECT_EQ(value->at(1).size(), 2U);
    EXPECT_EQ(value->at(1).at(0), 2);
    EXPECT_EQ(value->at(1).at(1), 3);
    EXPECT_EQ(value->at(4).size(), 1U);
    EXPECT_EQ(value->at(4).at(0), 5);
}

TEST_CASE(serialize_struct) {
    person_yy value{};
    value.id = 7;
    value.name = "alice";
    value.scores = {10, 20, 30};

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto dom = serializer.dom();
    ASSERT_TRUE(dom.has_value());
    auto out = dom->str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"id":7,"name":"alice","scores":[10,20,30]})");
}

TEST_CASE(deserialize_struct) {
    std::string json = R"({"id":9,"name":"bob","scores":[4,5]})";

    auto dom = eventide::serde::json::yy::parse(json);
    ASSERT_TRUE(dom.has_value());

    deserializer_t deserializer(std::move(*dom));

    auto value = eventide::serde::deserialize<person_yy>(deserializer);
    ASSERT_TRUE(value.has_value());

    EXPECT_EQ(value->id, 9);
    EXPECT_EQ(value->name, "bob");
    ASSERT_EQ(value->scores.size(), 2U);
    EXPECT_EQ(value->scores.at(0), 4);
    EXPECT_EQ(value->scores.at(1), 5);
}

TEST_CASE(parse_returns_error) {
    std::string json = R"({)";

    auto parsed = eventide::serde::json::yy::parse(json);

    EXPECT_FALSE(parsed.has_value());
    if(!parsed.has_value()) {
        EXPECT_NE(parsed.error(), YYJSON_READ_SUCCESS);
    }
}

TEST_CASE(serialize_to_dom_and_deserialize_from_dom) {
    person_yy value{};
    value.id = 42;
    value.name = "dom";
    value.scores = {3, 1, 4};

    serializer_t serializer;
    eventide::serde::serialize(serializer, value);

    auto dom = serializer.dom();
    ASSERT_TRUE(dom.has_value());

    auto dom_json = dom->str();
    ASSERT_TRUE(dom_json.has_value());
    EXPECT_EQ(*dom_json, R"({"id":42,"name":"dom","scores":[3,1,4]})");

    deserializer_t deserializer(std::move(*dom));

    auto roundtrip = eventide::serde::deserialize<person_yy>(deserializer);
    ASSERT_TRUE(roundtrip.has_value());
    EXPECT_EQ(roundtrip->id, 42);
    EXPECT_EQ(roundtrip->name, "dom");
    ASSERT_EQ(roundtrip->scores.size(), 3U);
    EXPECT_EQ(roundtrip->scores.at(0), 3);
    EXPECT_EQ(roundtrip->scores.at(1), 1);
    EXPECT_EQ(roundtrip->scores.at(2), 4);
}

TEST_CASE(visitor_deserialize_any_bool) {
    auto dom = eventide::serde::json::yy::parse(R"(true)");
    ASSERT_TRUE(dom.has_value());

    deserializer_t deserializer(std::move(*dom));

    any_bool_visitor visitor{};
    auto value = deserializer.deserialize_any(visitor);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, true);
}

TEST_CASE(visitor_deserialize_seq_sum) {
    auto dom = eventide::serde::json::yy::parse(R"([1,2,3,4])");
    ASSERT_TRUE(dom.has_value());

    deserializer_t deserializer(std::move(*dom));

    seq_sum_visitor visitor{};
    auto value = deserializer.deserialize_seq(visitor);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 10);
}

TEST_CASE(visitor_deserialize_map_sum) {
    auto dom = eventide::serde::json::yy::parse(R"({"a":2,"b":3,"c":5})");
    ASSERT_TRUE(dom.has_value());

    deserializer_t deserializer(std::move(*dom));

    map_sum_visitor visitor{};
    auto value = deserializer.deserialize_map(visitor);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 10);
}

TEST_CASE(dom_read_helpers) {
    auto dom = eventide::serde::json::yy::parse(
        R"({"name":"alice","age":20,"scores":[10,20],"active":true})");
    ASSERT_TRUE(dom.has_value());

    ASSERT_TRUE(dom->is_object());
    EXPECT_EQ(dom->object_size(), 4U);

    auto* name_node = dom->get("name");
    ASSERT_TRUE(name_node != nullptr);
    ASSERT_TRUE(dom->is_string(name_node));

    auto name = dom->as_string(name_node);
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "alice");

    auto age = dom->as_i64(dom->get("age"));
    ASSERT_TRUE(age.has_value());
    EXPECT_EQ(*age, 20);

    auto active = dom->as_bool(dom->get("active"));
    ASSERT_TRUE(active.has_value());
    EXPECT_EQ(*active, true);

    auto* scores = dom->get("scores");
    ASSERT_TRUE(scores != nullptr);
    ASSERT_TRUE(dom->is_array(scores));
    EXPECT_EQ(dom->array_size(scores), 2U);
    auto first_score = dom->as_i64(dom->at(0, scores));
    ASSERT_TRUE(first_score.has_value());
    EXPECT_EQ(*first_score, 10);
}

TEST_CASE(dom_object_mutation_helpers) {
    auto dom = eventide::serde::json::yy::MutableDom::object();
    ASSERT_TRUE(dom.has_value());

    ASSERT_TRUE(dom->set_string("name", "alice").has_value());
    ASSERT_TRUE(dom->set_i64("age", 20).has_value());
    ASSERT_TRUE(dom->set_bool("active", true).has_value());
    ASSERT_TRUE(dom->erase("active").has_value());

    auto json = dom->str();
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ(*json, R"({"name":"alice","age":20})");
}

TEST_CASE(dom_array_mutation_helpers) {
    auto dom = eventide::serde::json::yy::MutableDom::array();
    ASSERT_TRUE(dom.has_value());

    ASSERT_TRUE(dom->push_i64(1).has_value());
    ASSERT_TRUE(dom->push_string("two").has_value());
    ASSERT_TRUE(dom->push_bool(true).has_value());
    ASSERT_TRUE(dom->remove_at(1).has_value());
    ASSERT_TRUE(dom->push_u64(3).has_value());

    auto json = dom->str();
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ(*json, R"([1,true,3])");
}

TEST_CASE(dom_thaw_freeze) {
    auto dom = eventide::serde::json::yy::parse(R"({"n":1,"arr":[2],"flag":false})");
    ASSERT_TRUE(dom.has_value());

    auto mutable_dom = dom->thaw();
    ASSERT_TRUE(mutable_dom.has_value());

    ASSERT_TRUE(mutable_dom->set_i64("n", 5).has_value());
    ASSERT_TRUE(mutable_dom->set_bool("flag", true).has_value());

    auto frozen = mutable_dom->freeze();
    ASSERT_TRUE(frozen.has_value());

    auto out = frozen->str();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, R"({"n":5,"arr":[2],"flag":true})");
}

};  // TEST_SUITE(serde_yyjson)

}  // namespace

}  // namespace eventide::serde
