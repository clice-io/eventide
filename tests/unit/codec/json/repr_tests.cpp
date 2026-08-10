#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/meta/repr.h"
#include "kota/codec/json/json.h"
#include "kota/codec/json/schema.h"

namespace kota_repr_test {

// The clice RelationKind pattern: an enum that must travel as a fixed-width
// unsigned integer, overriding the built-in enum dispatch.
enum class relation : std::uint8_t {
    declares,
    defines,
    references,
};

// A value class with a textual wire form ("major.minor").
struct version {
    int major = 0;
    int minor = 0;

    auto operator<=>(const version&) const = default;
};

// Encode-only type: decoding it must never be instantiated.
struct audit_stamp {
    std::uint64_t at = 0;
};

// Imperative form: the body drives the visitor, wire shape stays declared.
struct hex_id {
    std::uint32_t v = 0;

    auto operator==(const hex_id&) const -> bool = default;
};

// Dynamic form: int or string, decided at runtime.
struct poly_value {
    std::variant<std::int64_t, std::string> v;

    auto operator==(const poly_value&) const -> bool = default;
};

}  // namespace kota_repr_test

namespace kota::meta {

template <>
struct repr<kota_repr_test::relation> {
    using type = std::uint32_t;

    static type to(kota_repr_test::relation r) {
        return static_cast<type>(r);
    }

    static kota_repr_test::relation from(type v) {
        return static_cast<kota_repr_test::relation>(v);
    }
};

template <>
struct repr<kota_repr_test::version> {
    using type = std::string;

    static type to(const kota_repr_test::version& v) {
        return std::format("{}.{}", v.major, v.minor);
    }

    static kota_repr_test::version from(const std::string& wire) {
        kota_repr_test::version v;
        auto dot = wire.find('.');
        std::from_chars(wire.data(), wire.data() + dot, v.major);
        std::from_chars(wire.data() + dot + 1, wire.data() + wire.size(), v.minor);
        return v;
    }
};

template <>
struct repr<kota_repr_test::audit_stamp> {
    using type = std::uint64_t;

    static type to(const kota_repr_test::audit_stamp& s) {
        return s.at;
    }
};

template <>
struct repr<kota_repr_test::hex_id> {
    using type = std::string;

    template <typename Config>
    static bool serialize(auto& vis, const kota_repr_test::hex_id& h) {
        return vis.visit_str(std::format("{:08x}", h.v));
    }

    template <typename Config>
    static bool deserialize(auto& vis, kota_repr_test::hex_id& h) {
        std::string s;
        if(!vis.visit_str(s))
            return false;
        h.v = static_cast<std::uint32_t>(std::stoul(s, nullptr, 16));
        return true;
    }
};

template <>
struct repr<kota_repr_test::poly_value> {
    using type = dynamic;

    template <typename Config>
    static bool serialize(auto& vis, const kota_repr_test::poly_value& p) {
        return std::visit([&](const auto& alt) { return codec::encode_value<Config>(vis, alt); },
                          p.v);
    }

    template <typename Config>
    static bool deserialize(auto& vis, kota_repr_test::poly_value& p) {
        if(vis.peek_kind() == type_kind::string) {
            std::string s;
            if(!vis.visit_str(s))
                return false;
            p.v = std::move(s);
            return true;
        }
        std::int64_t n = 0;
        if(!vis.visit_int(n))
            return false;
        p.v = n;
        return true;
    }
};

}  // namespace kota::meta

namespace kota::codec {

namespace {

using json::from_json;
using json::to_json;
using kota_repr_test::audit_stamp;
using kota_repr_test::hex_id;
using kota_repr_test::poly_value;
using kota_repr_test::relation;
using kota_repr_test::version;

struct symbol {
    relation rel = relation::declares;
    version ver;

    auto operator==(const symbol&) const -> bool = default;
};

// Field annotation must beat the field type's repr.
struct version_as_int_adapter {
    using type = std::uint32_t;

    static std::uint32_t to(const version& v) {
        return static_cast<std::uint32_t>(v.major * 1000 + v.minor);
    }

    static version from(std::uint32_t wire) {
        return {.major = static_cast<int>(wire / 1000), .minor = static_cast<int>(wire % 1000)};
    }
};

struct packed_symbol {
    meta::annotation<version, meta::behavior::with<version_as_int_adapter>> ver;
};

struct audit_log {
    audit_stamp stamp;
};

struct dynamic_holder {
    poly_value v;
};

struct maybe_version {
    std::optional<version> v;
};

/// Imperative adapter: uppercases on the wire, lowercases back.
struct shout_adapter {
    using type = std::string;

    template <typename Config>
    static bool serialize(auto& vis, const std::string& s) {
        std::string wire = s;
        for(char& c: wire)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return vis.visit_str(wire);
    }

    template <typename Config>
    static bool deserialize(auto& vis, std::string& s) {
        std::string wire;
        if(!vis.visit_str(wire))
            return false;
        for(char& c: wire)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        s = std::move(wire);
        return true;
    }
};

struct shouted {
    meta::annotation<std::string, meta::behavior::with<shout_adapter>> name;
};

struct string_enum_config {
    [[maybe_unused]] constexpr static auto enum_repr = codec::enum_repr::String;
};

TEST_SUITE(serde_json_repr) {

TEST_CASE(declarative_repr_roundtrip) {
    const symbol input{
        .rel = relation::references,
        .ver = {.major = 1, .minor = 22}
    };

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"rel":2,"ver":"1.22"})");

    symbol output{};
    auto status = from_json(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(repr_reaches_container_elements_and_map_keys) {
    std::vector<relation> rels{relation::defines, relation::declares};
    auto encoded = to_json(rels);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"([1,0])");

    std::vector<relation> parsed_rels;
    ASSERT_TRUE(from_json(*encoded, parsed_rels).has_value());
    EXPECT_EQ(parsed_rels, rels);

    // A repr with a string wire shape makes the type usable as a JSON map key.
    std::map<version, int> by_version{
        {{.major = 1, .minor = 0}, 10},
        {{.major = 2, .minor = 5}, 25},
    };
    auto encoded_map = to_json(by_version);
    ASSERT_TRUE(encoded_map.has_value());
    EXPECT_EQ(*encoded_map, R"({"1.0":10,"2.5":25})");

    std::map<version, int> parsed_map;
    ASSERT_TRUE(from_json(*encoded_map, parsed_map).has_value());
    EXPECT_EQ(parsed_map, by_version);
}

TEST_CASE(field_annotation_beats_type_repr) {
    const packed_symbol input{.ver = {{.major = 3, .minor = 14}}};

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"ver":3014})");

    packed_symbol output{};
    auto status = from_json(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(meta::annotated_value(output.ver), (version{.major = 3, .minor = 14}));
}

TEST_CASE(one_directional_repr_encodes) {
    const audit_log input{.stamp = {.at = 1234567}};

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"stamp":1234567})");
}

TEST_CASE(imperative_repr_roundtrip) {
    const hex_id input{.v = 0xDEADBEEF};

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("deadbeef")");

    hex_id output{};
    auto status = from_json(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(dynamic_repr_roundtrip) {
    dynamic_holder as_int{.v = {.v = std::int64_t{42}}};
    auto encoded = to_json(as_int);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"v":42})");

    dynamic_holder output{};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output.v, as_int.v);

    dynamic_holder as_str{.v = {.v = std::string("free-form")}};
    encoded = to_json(as_str);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"v":"free-form"})");

    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output.v, as_str.v);
}

TEST_CASE(repr_alternative_in_untagged_variant) {
    // The version alternative arrives as its string wire shape; alternative
    // pruning must judge compatibility against that, not the raw kind.
    std::variant<version, int> input = version{.major = 1, .minor = 22};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("1.22")");

    std::variant<version, int> output;
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output, input);

    input = 7;
    encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(repr_inside_optional) {
    maybe_version input{
        .v = version{.major = 1, .minor = 5}
    };
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"v":"1.5"})");

    maybe_version output{};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output.v, input.v);

    input.v.reset();
    encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    output.v = version{.major = 9, .minor = 9};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_FALSE(output.v.has_value());
}

TEST_CASE(repr_beats_enum_string_config) {
    // The repr'd enum still travels as its declared integer wire shape.
    auto encoded = to_json<string_enum_config>(relation::references);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, "2");
}

TEST_CASE(imperative_with_adapter_roundtrip) {
    shouted input{.name = "loud"};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"name":"LOUD"})");

    shouted output{};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(meta::annotated_value(output.name), std::string("loud"));
}

TEST_CASE(schema_follows_repr) {
    auto schema = json::schema_string<symbol>();
    ASSERT_TRUE(schema.has_value());

    // relation surfaces as its uint32 wire shape, version as a string.
    EXPECT_TRUE(schema->find(R"("rel":{"type":"integer")") != std::string::npos);
    EXPECT_TRUE(schema->find(R"("ver":{"type":"string"})") != std::string::npos);
    EXPECT_TRUE(schema->find("enum") == std::string::npos);

    // A dynamic repr degrades to the "any" schema.
    auto dynamic_schema = json::schema_string<dynamic_holder>();
    ASSERT_TRUE(dynamic_schema.has_value());
    EXPECT_TRUE(dynamic_schema->find(R"("v":{})") != std::string::npos);
}

};  // TEST_SUITE(serde_json_repr)

}  // namespace

}  // namespace kota::codec
