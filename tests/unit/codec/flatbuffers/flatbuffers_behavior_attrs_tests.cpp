#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "kota/zest/zest.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/codec/flatbuffers/flatbuffers.h"

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

#endif
