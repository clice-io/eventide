#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/repr.h"
#include "kota/codec/bincode/bincode.h"

namespace kota_bincode_repr_test {

enum class flavor : std::uint8_t {
    plain,
    spicy,
};

// The roaring::Roaring pattern: a third-party value class that frames itself
// as a byte blob. The shape is declared dynamic; on a streaming backend the
// user body stays symmetric by writing/reading one length-prefixed vector.
struct blob_bag {
    std::vector<std::byte> bytes;

    auto operator==(const blob_bag&) const -> bool = default;
};

}  // namespace kota_bincode_repr_test

namespace kota::meta {

template <>
struct repr<kota_bincode_repr_test::flavor> {
    using type = std::uint32_t;

    static type to(kota_bincode_repr_test::flavor f) {
        return static_cast<type>(f);
    }

    static kota_bincode_repr_test::flavor from(type v) {
        return static_cast<kota_bincode_repr_test::flavor>(v);
    }
};

template <>
struct repr<kota_bincode_repr_test::blob_bag> {
    using type = dynamic;

    template <typename Config>
    static bool serialize(auto& vis, const kota_bincode_repr_test::blob_bag& b) {
        return codec::encode_value<Config>(vis, b.bytes);
    }

    template <typename Config>
    static bool deserialize(auto& vis, kota_bincode_repr_test::blob_bag& b) {
        std::vector<std::byte> bytes;
        if(!codec::decode_value<Config>(vis, bytes))
            return false;
        b.bytes = std::move(bytes);
        return true;
    }
};

}  // namespace kota::meta

namespace kota::codec {

namespace {

using kota_bincode_repr_test::blob_bag;
using kota_bincode_repr_test::flavor;

struct order {
    flavor f = flavor::plain;
    blob_bag payload;
    std::string note;

    auto operator==(const order&) const -> bool = default;
};

TEST_SUITE(serde_bincode_repr) {

TEST_CASE(declarative_and_dynamic_repr_roundtrip) {
    const order input{
        .f = flavor::spicy,
        .payload = blob_bag{{std::byte{0xAB}, std::byte{0xCD}}},
        .note = "extra",
    };

    auto bytes = bincode::to_bytes(input);
    ASSERT_TRUE(bytes.has_value());

    order output{};
    auto status = bincode::from_bytes(*bytes, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

TEST_CASE(repr_reaches_sequence_elements) {
    const std::vector<flavor> input{flavor::spicy, flavor::plain, flavor::spicy};

    auto bytes = bincode::to_bytes(input);
    ASSERT_TRUE(bytes.has_value());

    std::vector<flavor> output;
    auto status = bincode::from_bytes(*bytes, output);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(output, input);
}

};  // TEST_SUITE(serde_bincode_repr)

}  // namespace

}  // namespace kota::codec
