#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/meta/repr.h"
#include "kota/meta/type_info.h"
#include "kota/codec/json/json.h"
#include "kota/codec/json/schema.h"
#include "kota/codec/macro.h"

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

// Chained repr: ticket travels as step_id, whose own repr is uint32.
struct step_id {
    std::uint32_t v = 0;

    auto operator==(const step_id&) const -> bool = default;
};

struct ticket {
    step_id id;

    auto operator==(const ticket&) const -> bool = default;
};

// A non-nullable value with a nullable wire shape (zero travels as null).
struct lamport_stamp {
    std::uint32_t tick = 0;

    auto operator==(const lamport_stamp&) const -> bool = default;
};

// Repr whose declared wire type is itself annotated: the annotation's
// behavior attr decides the final wire shape.
struct basis_points {
    int v = 0;

    auto operator==(const basis_points&) const -> bool = default;
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
        if(dot == std::string::npos) {
            std::from_chars(wire.data(), wire.data() + wire.size(), v.major);
            return v;
        }
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

template <>
struct repr<kota_repr_test::step_id> {
    using type = std::uint32_t;

    static type to(kota_repr_test::step_id s) {
        return s.v;
    }

    static kota_repr_test::step_id from(type v) {
        return {.v = v};
    }
};

template <>
struct repr<kota_repr_test::ticket> {
    using type = kota_repr_test::step_id;

    static type to(const kota_repr_test::ticket& t) {
        return t.id;
    }

    static kota_repr_test::ticket from(type id) {
        return {.id = id};
    }
};

template <>
struct repr<kota_repr_test::lamport_stamp> {
    using type = std::optional<std::uint32_t>;

    static type to(const kota_repr_test::lamport_stamp& s) {
        return s.tick == 0 ? type{} : type{s.tick};
    }

    static kota_repr_test::lamport_stamp from(type v) {
        return {.tick = v.value_or(0)};
    }
};

template <>
struct repr<kota_repr_test::basis_points> {
    using type = annotation<int, behavior::as<double>>;

    static type to(const kota_repr_test::basis_points& b) {
        return b.v;
    }

    static kota_repr_test::basis_points from(const type& v) {
        return {.v = annotated_value(v)};
    }
};

}  // namespace kota::meta

namespace kota::codec {

namespace {

using json::from_json;
using json::to_json;
using kota_repr_test::audit_stamp;
using kota_repr_test::basis_points;
using kota_repr_test::hex_id;
using kota_repr_test::lamport_stamp;
using kota_repr_test::poly_value;
using kota_repr_test::relation;
using kota_repr_test::ticket;
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

struct chained_holder {
    ticket t;

    auto operator==(const chained_holder&) const -> bool = default;
};

struct stamped {
    lamport_stamp s;

    auto operator==(const stamped&) const -> bool = default;
};

struct fee_schedule {
    basis_points fee;

    auto operator==(const fee_schedule&) const -> bool = default;
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

// One annotation carrying both a variant tagging spec and a with-adapter: the
// adapter decides the wire shape (as in meta::wire_type_t), the tagging spec
// is inert.
struct choice_text_adapter {
    using type = std::string;

    static auto to(const std::variant<int, std::string>& v) -> std::string {
        if(const auto* n = std::get_if<int>(&v)) {
            return std::format("i:{}", *n);
        }
        return std::format("s:{}", std::get<std::string>(v));
    }

    static auto from(const std::string& wire) -> std::variant<int, std::string> {
        if(wire.starts_with("i:")) {
            int n = 0;
            std::from_chars(wire.data() + 2, wire.data() + wire.size(), n);
            return n;
        }
        return wire.starts_with("s:") ? wire.substr(2) : wire;
    }
};

KOTATSU_ANNOTATION(tagged_choice_annotation, tag = "t", content = "c", tag_names = {"num", "text"});
using adapted_tagged_choice =
    meta::annotate<tagged_choice_annotation>::type<std::variant<int, std::string>,
                                                   meta::behavior::with<choice_text_adapter>>;

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

TEST_CASE(chained_repr_resolves_to_final_wire_shape) {
    const chained_holder input{.t = {.id = {.v = 7}}};

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"t":7})");

    chained_holder output{};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output, input);

    // The schema follows the chain to the final integer shape.
    auto schema = json::schema_string<chained_holder>();
    ASSERT_TRUE(schema.has_value());
    EXPECT_TRUE(schema->find(R"("t":{"type":"integer")") != std::string::npos);
}

TEST_CASE(annotation_nested_in_repr_wire_type) {
    // The repr's declared wire type carries a behavior attr; the resolver
    // must follow it to the annotation's wire shape, so schema consumers
    // classify the double the dispatch actually writes.
    static_assert(std::is_same_v<meta::wire_type_t<basis_points>, double>);

    const fee_schedule input{.fee = {.v = 250}};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());

    fee_schedule output{};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output, input);

    auto schema = json::schema_string<fee_schedule>();
    ASSERT_TRUE(schema.has_value());
    EXPECT_TRUE(schema->find(R"("fee":{"type":"number"})") != std::string::npos);
}

TEST_CASE(annotated_repr_alternative_in_untagged_variant) {
    // The adapter, not version's own string repr, decides the wire shape, so
    // alternative pruning must keep the numeric alternative on number input.
    using packed_ver = meta::annotation<version, meta::behavior::with<version_as_int_adapter>>;
    std::variant<packed_ver, std::string> input = packed_ver{
        {.major = 3, .minor = 14}
    };

    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, "3014");

    std::variant<packed_ver, std::string> output;
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    ASSERT_TRUE(output.index() == 0);
    EXPECT_EQ(meta::annotated_value(std::get<0>(output)), (version{.major = 3, .minor = 14}));
}

TEST_CASE(adapter_beats_variant_tagging_outside_fields) {
    // The wire-type resolver gives the adapter precedence over the tagging
    // spec; the top-level value dispatch must agree with it (and with the
    // field-level dispatch), not emit a tagged object.
    static_assert(std::is_same_v<meta::wire_type_t<adapted_tagged_choice>, std::string>);

    adapted_tagged_choice input{7};
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"("i:7")");

    adapted_tagged_choice output{};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(std::get<int>(meta::annotated_value(output)), 7);
}

TEST_CASE(nullable_wire_shape_keeps_field_required) {
    // The wire value may be null, but the property itself must be present:
    // requiredness follows the declared field type, which decode enforces.
    auto schema = json::schema_string<stamped>();
    ASSERT_TRUE(schema.has_value());
    EXPECT_TRUE(schema->find(R"("required":["s"])") != std::string::npos);

    const stamped input{};  // tick == 0 travels as null
    auto encoded = to_json(input);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(*encoded, R"({"s":null})");

    stamped output{.s = {.tick = 9}};
    ASSERT_TRUE(from_json(*encoded, output).has_value());
    EXPECT_EQ(output, input);

    // An absent property is rejected, matching the schema.
    EXPECT_FALSE(from_json("{}", output).has_value());
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
