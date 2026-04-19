#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstdint>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/codec/flatbuffers/flatbuffers.h"
#include "kota/codec/flatbuffers/proxy.h"
#include "kota/codec/traits.h"

namespace kota::codec {

using namespace meta;

namespace {

using flatbuffers::from_flatbuffer;
using flatbuffers::to_flatbuffer;

enum class role : std::int32_t {
    admin,
    editor,
    viewer,
};

// Adapter: encode int on the wire as its decimal string representation.
struct IntStringAdapter {
    using wire_type = std::string;

    static auto to_wire(int value) -> std::string {
        return std::to_string(value);
    }

    static auto from_wire(std::string wire) -> int {
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

TEST_CASE(with_adapter_roundtrip_inside_optional) {
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
// Type-level adapter tests: codec::type_adapter<T>
// ----------------------------------------------------------------------------
// Verifies that a type-level adapter specialization propagates through the
// arena codec dispatch AND the flatbuffers proxy layer, without requiring
// per-field `annotation<T, with<...>>`. Covers:
//   - direct field use
//   - map value position
//   - sequence element position
//   - lazy proxy access via table_view / map_view
// ============================================================================

namespace kota_test_type_adapter {

// A value-class wrapping an integer — not trivially reflectable, not an
// enum, and final (so kota's meta::annotation wrap_type path would apply).
// The adapter encodes it as a plain uint32_t on the wire.
class Tag final {
public:
    Tag() = default;
    explicit Tag(std::uint32_t v) : value_(v) {}
    auto value() const -> std::uint32_t { return value_; }
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
    auto bytes() const -> const std::vector<std::byte>& { return bytes_; }
    auto operator==(const ByteBag&) const -> bool = default;

private:
    std::vector<std::byte> bytes_;
};

}  // namespace kota_test_type_adapter

// Type-level adapter specializations (placed in kota::codec namespace as
// required by the trait's declaration).
namespace kota::codec {

template <>
struct type_adapter<kota_test_type_adapter::Tag> {
    using wire_type = std::uint32_t;

    static auto to_wire(const kota_test_type_adapter::Tag& tag) -> std::uint32_t {
        return tag.value();
    }

    static auto from_wire(std::uint32_t wire) -> kota_test_type_adapter::Tag {
        return kota_test_type_adapter::Tag{wire};
    }
};

template <>
struct type_adapter<kota_test_type_adapter::ByteBag> {
    using wire_type = std::vector<std::byte>;

    static auto to_wire(const kota_test_type_adapter::ByteBag& bag) -> std::vector<std::byte> {
        return bag.bytes();
    }

    static auto from_wire(std::vector<std::byte> wire) -> kota_test_type_adapter::ByteBag {
        return kota_test_type_adapter::ByteBag{std::move(wire)};
    }
};

}  // namespace kota::codec

namespace kota::codec {

namespace {

using kota_test_type_adapter::Tag;
using kota_test_type_adapter::ByteBag;

struct TypeAdapterPlainField {
    Tag tag;
    std::string label;

    auto operator==(const TypeAdapterPlainField&) const -> bool = default;
};

struct TypeAdapterMapField {
    std::map<std::uint32_t, Tag> tags_by_id;
    std::map<std::uint32_t, ByteBag> blobs_by_id;

    auto operator==(const TypeAdapterMapField&) const -> bool = default;
};

struct TypeAdapterSequenceField {
    std::vector<Tag> tags;

    auto operator==(const TypeAdapterSequenceField&) const -> bool = default;
};

struct TypeAdapterRoot {
    Tag root_tag;
    std::map<std::uint32_t, ByteBag> blobs;
    std::string content;

    auto operator==(const TypeAdapterRoot&) const -> bool = default;
};

TEST_SUITE(serde_flatbuffers_type_adapter) {

TEST_CASE(type_adapter_plain_field_roundtrip) {
    const TypeAdapterPlainField input{.tag = Tag{42}, .label = "hello"};

    auto encoded = flatbuffers::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    TypeAdapterPlainField output{};
    auto status = flatbuffers::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(type_adapter_map_value_roundtrip) {
    TypeAdapterMapField input;
    input.tags_by_id[1] = Tag{100};
    input.tags_by_id[2] = Tag{200};
    input.blobs_by_id[10] = ByteBag{{std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}}};
    input.blobs_by_id[20] = ByteBag{{std::byte{0x11}}};

    auto encoded = flatbuffers::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    TypeAdapterMapField output{};
    auto status = flatbuffers::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(type_adapter_sequence_element_roundtrip) {
    TypeAdapterSequenceField input;
    input.tags = {Tag{1}, Tag{2}, Tag{3}};

    auto encoded = flatbuffers::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    TypeAdapterSequenceField output{};
    auto status = flatbuffers::from_flatbuffer(*encoded, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(type_adapter_proxy_lazy_scalar_access) {
    const TypeAdapterRoot input{.root_tag = Tag{777},
                                .blobs = {},
                                .content = "lazy"};
    auto encoded = flatbuffers::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = flatbuffers::table_view<TypeAdapterRoot>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());

    // Proxy sees the wire type (uint32_t) — the user calls from_wire
    // themselves when they need the adapted type. Equivalent to:
    //   adapter::from_wire(root[&TypeAdapterRoot::root_tag])
    const std::uint32_t wire_tag = root[&TypeAdapterRoot::root_tag];
    EXPECT_EQ(wire_tag, 777U);

    const std::string_view content = root[&TypeAdapterRoot::content];
    EXPECT_EQ(content, std::string_view{"lazy"});
}

TEST_CASE(type_adapter_proxy_lazy_map_value_access) {
    TypeAdapterRoot input;
    input.root_tag = Tag{1};
    input.blobs[5] = ByteBag{{std::byte{0xDE}, std::byte{0xAD}}};
    input.blobs[9] = ByteBag{{std::byte{0xBE}, std::byte{0xEF}}};

    auto encoded = flatbuffers::to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = flatbuffers::table_view<TypeAdapterRoot>::from_bytes(
        std::span<const std::uint8_t>(encoded->data(), encoded->size()));
    ASSERT_TRUE(root.valid());

    auto blobs = root[&TypeAdapterRoot::blobs];
    ASSERT_TRUE(blobs.valid());
    EXPECT_EQ(blobs.size(), 2U);

    // map_view<K, ByteBag> — proxy substitutes wire_type (vector<byte>),
    // so operator[] returns an array_view<std::byte>.
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

};  // TEST_SUITE(serde_flatbuffers_type_adapter)

}  // namespace

}  // namespace kota::codec

#endif
