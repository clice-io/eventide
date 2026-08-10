#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/codec/fbs/fbs.h"

namespace kota::codec {

using namespace meta;

namespace {

using fbs::from_flatbuffer;
using fbs::to_flatbuffer;

enum class role : std::int32_t {
    admin,
    editor,
    viewer,
};

// Adapter: encode int on the wire as its decimal string representation.
struct IntStringAdapter {
    using type = std::string;

    static auto to(int value) -> std::string {
        return std::to_string(value);
    }

    static auto from(std::string wire) -> int {
        return wire.empty() ? 0 : std::stoi(wire);
    }
};

struct with_enum_string_field {
    std::int32_t id = 0;
    annotation<role, behavior::enum_string<rename_policy::identity>> level{role::admin};

    auto operator==(const with_enum_string_field&) const -> bool = default;
};

struct with_adapter_field {
    std::int32_t id = 0;
    annotation<int, behavior::with<IntStringAdapter>> encoded = 0;
    std::string tag;

    auto operator==(const with_adapter_field&) const -> bool = default;
};

struct with_optional_adapter_field {
    std::optional<annotation<int, behavior::with<IntStringAdapter>>> maybe_encoded;

    auto operator==(const with_optional_adapter_field&) const -> bool = default;
};

TEST_SUITE(serde_flatbuffers_behavior_attrs) {

TEST_CASE(enum_string_roundtrip_on_struct_field) {
    const with_enum_string_field input{.id = 42, .level = role::editor};

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    with_enum_string_field output{};
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(enum_string_roundtrip_viewer_value) {
    const with_enum_string_field input{.id = 7, .level = role::viewer};

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    with_enum_string_field output{};
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(with_adapter_roundtrip_int_as_string) {
    const with_adapter_field input{.id = 9, .encoded = 12345, .tag = "gold"};

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    with_adapter_field output{};
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(with_adapter_roundtrip_negative_value) {
    const with_adapter_field input{.id = 1, .encoded = -42, .tag = "debt"};

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    with_adapter_field output{};
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(with_adapter_roundtrip_inside_optional, skip = true) {
    with_optional_adapter_field input{};
    input.maybe_encoded.emplace(7);

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    with_optional_adapter_field output{};
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(output.maybe_encoded.has_value());
    EXPECT_EQ(annotated_value(*output.maybe_encoded), 7);
}

TEST_CASE(with_adapter_roundtrip_empty_optional) {
    const with_optional_adapter_field input{};

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    with_optional_adapter_field output{};
    output.maybe_encoded.emplace(999);  // ensure decode clears it
    auto status = from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_FALSE(output.maybe_encoded.has_value());
}

};  // TEST_SUITE(serde_flatbuffers_behavior_attrs)

}  // namespace

}  // namespace kota::codec

// ============================================================================
// Type-level traits tests: meta::repr
// ============================================================================

namespace kota_test_type_traits {

// A value-class wrapping an integer — not trivially reflectable, not an
// enum, and final (so kota's meta::annotation wrap_type path would apply).
// The adapter encodes it as a plain uint32_t on the wire.
class Tag final {
public:
    Tag() = default;

    explicit Tag(std::uint32_t v) : value_(v) {}

    auto value() const -> std::uint32_t {
        return value_;
    }

    auto operator==(const Tag&) const -> bool = default;
    auto operator<=>(const Tag&) const = default;

private:
    std::uint32_t value_ = 0;
};

// A value-class wrapping a byte sequence — emulates the roaring::Roaring
// shape (third-party class serialized as an opaque byte blob).
class ByteBag {
public:
    ByteBag() = default;

    explicit ByteBag(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}

    auto bytes() const -> const std::vector<std::byte>& {
        return bytes_;
    }

    auto operator==(const ByteBag&) const -> bool = default;

private:
    std::vector<std::byte> bytes_;
};

// An integer id whose repr is imperative: the body drives the visitor, the
// declared string shape keeps the fbs layout honest.
class HexTag {
public:
    HexTag() = default;

    explicit HexTag(std::uint32_t v) : value_(v) {}

    auto value() const -> std::uint32_t {
        return value_;
    }

    auto operator==(const HexTag&) const -> bool = default;

private:
    std::uint32_t value_ = 0;
};

// An iterable value class (type_kind::array) with a scalar-string repr — the
// roaring-bitmap shape. Encode/decode of vector<IdSet> must agree that the
// element travels unwrapped as its wire string.
class IdSet {
public:
    IdSet() = default;

    explicit IdSet(std::vector<std::uint32_t> ids) : ids_(std::move(ids)) {}

    auto begin() const {
        return ids_.begin();
    }

    auto end() const {
        return ids_.end();
    }

    auto operator==(const IdSet&) const -> bool = default;

private:
    std::vector<std::uint32_t> ids_;
};

// A wire shape that must travel as a table: the string member rules out the
// inline-struct fast path, so vector elements need the table collectors.
struct EndpointWire {
    std::string host;
    std::uint32_t port = 0;
};

class Endpoint {
public:
    Endpoint() = default;

    Endpoint(std::string host, std::uint32_t port) : host_(std::move(host)), port_(port) {}

    auto host() const -> const std::string& {
        return host_;
    }

    auto port() const -> std::uint32_t {
        return port_;
    }

    auto operator==(const Endpoint&) const -> bool = default;

private:
    std::string host_;
    std::uint32_t port_ = 0;
};

// A nullable wire shape: vector elements must be boxed like plain optionals.
class MaybeId {
public:
    MaybeId() = default;

    explicit MaybeId(std::optional<std::uint32_t> v) : value_(v) {}

    auto value() const -> std::optional<std::uint32_t> {
        return value_;
    }

    auto operator==(const MaybeId&) const -> bool = default;

private:
    std::optional<std::uint32_t> value_;
};

}  // namespace kota_test_type_traits

namespace kota::meta {

template <>
struct repr<kota_test_type_traits::Tag> {
    using type = std::uint32_t;

    static type to(const kota_test_type_traits::Tag& tag) {
        return tag.value();
    }

    static kota_test_type_traits::Tag from(type v) {
        return kota_test_type_traits::Tag{v};
    }
};

template <>
struct repr<kota_test_type_traits::ByteBag> {
    using type = std::vector<std::byte>;

    const static type& to(const kota_test_type_traits::ByteBag& bag) {
        return bag.bytes();
    }

    static kota_test_type_traits::ByteBag from(type bytes) {
        return kota_test_type_traits::ByteBag{std::move(bytes)};
    }
};

template <>
struct repr<kota_test_type_traits::HexTag> {
    using type = std::string;

    template <typename Config>
    static bool serialize(auto& vis, const kota_test_type_traits::HexTag& tag) {
        return vis.visit_str(std::to_string(tag.value()));
    }

    template <typename Config>
    static bool deserialize(auto& vis, kota_test_type_traits::HexTag& tag) {
        std::string wire;
        if(!vis.visit_str(wire))
            return false;
        tag = kota_test_type_traits::HexTag{static_cast<std::uint32_t>(std::stoul(wire))};
        return true;
    }
};

template <>
struct repr<kota_test_type_traits::IdSet> {
    using type = std::string;

    static type to(const kota_test_type_traits::IdSet& set) {
        std::string wire;
        for(auto id: set) {
            if(!wire.empty())
                wire += ',';
            wire += std::to_string(id);
        }
        return wire;
    }

    static kota_test_type_traits::IdSet from(const std::string& wire) {
        std::vector<std::uint32_t> ids;
        std::size_t pos = 0;
        while(pos < wire.size()) {
            auto comma = wire.find(',', pos);
            if(comma == std::string::npos)
                comma = wire.size();
            ids.push_back(static_cast<std::uint32_t>(std::stoul(wire.substr(pos, comma - pos))));
            pos = comma + 1;
        }
        return kota_test_type_traits::IdSet{std::move(ids)};
    }
};

template <>
struct repr<kota_test_type_traits::Endpoint> {
    using type = kota_test_type_traits::EndpointWire;

    static type to(const kota_test_type_traits::Endpoint& e) {
        return {.host = e.host(), .port = e.port()};
    }

    static kota_test_type_traits::Endpoint from(type wire) {
        return {std::move(wire.host), wire.port};
    }
};

template <>
struct repr<kota_test_type_traits::MaybeId> {
    using type = std::optional<std::uint32_t>;

    static type to(const kota_test_type_traits::MaybeId& m) {
        return m.value();
    }

    static kota_test_type_traits::MaybeId from(type v) {
        return kota_test_type_traits::MaybeId{v};
    }
};

}  // namespace kota::meta

namespace kota::codec {

namespace {

using kota_test_type_traits::Tag;
using kota_test_type_traits::ByteBag;
using kota_test_type_traits::HexTag;
using kota_test_type_traits::IdSet;
using kota_test_type_traits::Endpoint;
using kota_test_type_traits::MaybeId;

struct TypeTraitsPlainField {
    Tag tag;
    std::string label;

    auto operator==(const TypeTraitsPlainField&) const -> bool = default;
};

struct TypeTraitsMapField {
    std::map<std::uint32_t, Tag> tags_by_id;
    std::map<std::uint32_t, ByteBag> blobs_by_id;

    auto operator==(const TypeTraitsMapField&) const -> bool = default;
};

struct TypeTraitsSequenceField {
    std::vector<Tag> tags;

    auto operator==(const TypeTraitsSequenceField&) const -> bool = default;
};

struct TypeTraitsRoot {
    Tag root_tag;
    std::map<std::uint32_t, ByteBag> blobs;
    std::string content;

    auto operator==(const TypeTraitsRoot&) const -> bool = default;
};

struct ImperativeReprField {
    HexTag tag;
    std::string label;

    auto operator==(const ImperativeReprField&) const -> bool = default;
};

struct IterableReprField {
    IdSet primary;
    std::vector<IdSet> groups;

    auto operator==(const IterableReprField&) const -> bool = default;
};

struct OptionalReprField {
    std::optional<Tag> maybe_tag;

    auto operator==(const OptionalReprField&) const -> bool = default;
};

struct TableWireReprField {
    std::vector<Endpoint> endpoints;

    auto operator==(const TableWireReprField&) const -> bool = default;
};

struct BoxedWireReprField {
    std::vector<MaybeId> ids;

    auto operator==(const BoxedWireReprField&) const -> bool = default;
};

struct BytesWireReprField {
    std::vector<ByteBag> blobs;

    auto operator==(const BytesWireReprField&) const -> bool = default;
};

// Adapter over a repr'd type: the field annotation must win over the type's
// own repr, on the wire and in the proxy view.
struct TagNameAdapter {
    using type = std::string;

    static auto to(const Tag& t) -> std::string {
        return std::to_string(t.value());
    }

    static auto from(const std::string& wire) -> Tag {
        return Tag{static_cast<std::uint32_t>(std::stoul(wire))};
    }
};

struct AdapterOverReprField {
    meta::annotation<Tag, meta::behavior::with<TagNameAdapter>> tag;
};

struct AdaptedElementField {
    std::vector<meta::annotation<int, meta::behavior::with<IntStringAdapter>>> vals;
};

TEST_SUITE(serde_flatbuffers_type_traits) {

TEST_CASE(type_traits_plain_field_roundtrip) {
    const TypeTraitsPlainField input{.tag = Tag{42}, .label = "hello"};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    TypeTraitsPlainField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(type_traits_map_value_roundtrip) {
    TypeTraitsMapField input;
    input.tags_by_id[1] = Tag{100};
    input.tags_by_id[2] = Tag{200};
    input.blobs_by_id[10] = ByteBag{
        {std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}}
    };
    input.blobs_by_id[20] = ByteBag{{std::byte{0x11}}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    TypeTraitsMapField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(type_traits_sequence_element_roundtrip) {
    TypeTraitsSequenceField input;
    input.tags = {Tag{1}, Tag{2}, Tag{3}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    TypeTraitsSequenceField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(type_traits_proxy_lazy_scalar_access) {
    const TypeTraitsRoot input{.root_tag = Tag{777}, .blobs = {}, .content = "lazy"};
    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = fbs::table_view<TypeTraitsRoot>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());

    // Proxy sees the wire type (uint32_t)
    const std::uint32_t wire_tag = root[&TypeTraitsRoot::root_tag];
    EXPECT_EQ(wire_tag, 777U);

    const std::string_view content = root[&TypeTraitsRoot::content];
    EXPECT_EQ(content, std::string_view{"lazy"});
}

TEST_CASE(type_traits_proxy_lazy_map_value_access) {
    TypeTraitsRoot input;
    input.root_tag = Tag{1};
    input.blobs[5] = ByteBag{
        {std::byte{0xDE}, std::byte{0xAD}}
    };
    input.blobs[9] = ByteBag{
        {std::byte{0xBE}, std::byte{0xEF}}
    };

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = fbs::table_view<TypeTraitsRoot>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());

    auto blobs = root[&TypeTraitsRoot::blobs];
    ASSERT_TRUE(blobs.valid());
    EXPECT_EQ(blobs.size(), 2U);

    // map_view<K, ByteBag> — proxy substitutes the repr wire shape
    // (vector<byte>), so operator[] returns an array_view<std::byte>.
    auto blob5 = blobs[5U];
    ASSERT_TRUE(blob5.valid());
    EXPECT_EQ(blob5.size(), 2U);
    EXPECT_EQ(blob5[0], std::byte{0xDE});
    EXPECT_EQ(blob5[1], std::byte{0xAD});

    auto blob9 = blobs[9U];
    ASSERT_TRUE(blob9.valid());
    EXPECT_EQ(blob9.size(), 2U);
    EXPECT_EQ(blob9[0], std::byte{0xBE});
    EXPECT_EQ(blob9[1], std::byte{0xEF});
}

TEST_CASE(imperative_repr_field_roundtrip) {
    const ImperativeReprField input{.tag = HexTag{54321}, .label = "imp"};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    ImperativeReprField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);

    // The wire carries the declared string shape, observable via the proxy.
    auto root = fbs::table_view<ImperativeReprField>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());
    const std::string_view wire_tag = root[&ImperativeReprField::tag];
    EXPECT_EQ(wire_tag, std::string_view{"54321"});
}

TEST_CASE(iterable_repr_element_roundtrip) {
    // IdSet's raw kind is array; its repr wire shape is a string. Encode and
    // decode of vector<IdSet> must agree the element is unwrapped.
    IterableReprField input;
    input.primary = IdSet{
        {1, 2, 3}
    };
    input.groups = {IdSet{{10, 20}}, IdSet{}, IdSet{{7}}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    IterableReprField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(repr_inside_optional_roundtrip) {
    OptionalReprField input{.maybe_tag = Tag{99}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    OptionalReprField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);

    input.maybe_tag.reset();
    encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    output.maybe_tag = Tag{1};
    status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_FALSE(output.maybe_tag.has_value());
}

TEST_CASE(table_wire_repr_element_roundtrip) {
    // Endpoint's wire shape is a table; vector elements must travel as table
    // offsets on both the encode and decode side.
    TableWireReprField input;
    input.endpoints = {
        Endpoint{"alpha", 1  },
        Endpoint{"",      0  },
        Endpoint{"beta",  443}
    };

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    TableWireReprField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(nullable_wire_repr_element_roundtrip) {
    // MaybeId's wire shape is optional; vector elements must be boxed exactly
    // like plain optional elements.
    BoxedWireReprField input;
    input.ids = {MaybeId{7U}, MaybeId{}, MaybeId{42U}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    BoxedWireReprField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(view_reads_boxed_nullable_repr_elements) {
    // Boxed elements are read through their wrapper table; the view peels the
    // nullable wire shape to the inner scalar, absence reads as its default.
    BoxedWireReprField input;
    input.ids = {MaybeId{7U}, MaybeId{}, MaybeId{42U}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = fbs::table_view<BoxedWireReprField>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());

    auto ids = root[&BoxedWireReprField::ids];
    ASSERT_TRUE(ids.valid());
    ASSERT_EQ(ids.size(), 3U);
    EXPECT_EQ(ids[0], 7U);
    EXPECT_EQ(ids[1], 0U);
    EXPECT_EQ(ids[2], 42U);
}

TEST_CASE(bytes_wire_repr_element_roundtrip) {
    // ByteBag's wire shape is a byte blob; flatbuffers has no
    // vector-of-vectors, so elements travel boxed in per-element wrapper
    // tables, matching plain nested byte containers.
    BytesWireReprField input;
    input.blobs = {ByteBag{{std::byte{0xAA}, std::byte{0xBB}}},
                   ByteBag{},
                   ByteBag{{std::byte{0x01}}}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    BytesWireReprField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);

    auto root = fbs::table_view<BytesWireReprField>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());

    auto blobs = root[&BytesWireReprField::blobs];
    ASSERT_TRUE(blobs.valid());
    ASSERT_EQ(blobs.size(), 3U);

    auto b0 = blobs[0];
    ASSERT_TRUE(b0.valid());
    ASSERT_EQ(b0.size(), 2U);
    EXPECT_EQ(b0[0], std::byte{0xAA});
    EXPECT_EQ(b0[1], std::byte{0xBB});
    EXPECT_EQ(blobs[1].size(), 0U);
}

TEST_CASE(adapted_element_travels_as_adapter_wire) {
    // The element's annotation adapter decides the wire shape (string), so
    // both sides must pick the string collectors, not the raw-int fast path.
    AdaptedElementField input;
    input.vals = {12, -3, 4567};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    AdaptedElementField output{};
    auto status = fbs::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(output.vals.size() == 3U);
    EXPECT_EQ(meta::annotated_value(output.vals[0]), 12);
    EXPECT_EQ(meta::annotated_value(output.vals[1]), -3);
    EXPECT_EQ(meta::annotated_value(output.vals[2]), 4567);
}

TEST_CASE(view_honors_field_adapter_over_type_repr) {
    // Tag's own repr is uint32, but the field adapter puts a string on the
    // wire; the proxy view must follow the adapter.
    const AdapterOverReprField input{.tag = Tag{4242}};

    auto encoded = fbs::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = fbs::table_view<AdapterOverReprField>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());
    const std::string_view wire_tag = root[&AdapterOverReprField::tag];
    EXPECT_EQ(wire_tag, std::string_view{"4242"});

    AdapterOverReprField output{};
    ASSERT_TRUE(fbs::from_flatbuffer(*encoded, output).has_value());
    EXPECT_EQ(meta::annotated_value(output.tag), Tag{4242});
}

};  // TEST_SUITE(serde_flatbuffers_type_traits)

}  // namespace

}  // namespace kota::codec

#endif
