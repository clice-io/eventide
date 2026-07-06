#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/fbs/fbs.h"
#include "flatbuffers/flatbuffers.h"

namespace kota::codec {

namespace {

/// An opaque value type the reflection framework cannot walk: private state,
/// no aggregate structure.
class ticket {
public:
    ticket() = default;

    explicit ticket(std::uint32_t id) : id_(id) {}

    std::uint32_t id() const {
        return id_;
    }

    bool operator==(const ticket&) const = default;

private:
    std::uint32_t id_ = 0;
};

/// Bridged through an owning wire type while to_wire hands out a view.
class label {
public:
    label() = default;

    explicit label(std::string text) : text_(std::move(text)) {}

    const std::string& text() const {
        return text_;
    }

    bool operator==(const label&) const = default;

private:
    std::string text_;
};

}  // namespace

/// Value-mode serialize_visit specializations: no visit() — wire_type
/// declares the on-wire layout, to_wire/from_wire convert, and one
/// specialization serves both encoding and decoding on every backend.
template <typename Vis, typename Config>
struct serialize_visit<Vis, ticket, Config> {
    using wire_type = std::uint32_t;

    static wire_type to_wire(const ticket& value) {
        return value.id();
    }

    static ticket from_wire(wire_type id) {
        return ticket(id);
    }
};

template <typename Vis, typename Config>
struct serialize_visit<Vis, label, Config> {
    using wire_type = std::string;

    static std::string_view to_wire(const label& value) {
        return value.text();
    }

    static label from_wire(wire_type text) {
        return label(std::move(text));
    }
};

namespace {

using fbs::from_flatbuffer;
using fbs::table_view;
using fbs::to_flatbuffer;

struct order {
    ticket id;
    label name;
    std::vector<ticket> history;
    std::map<std::uint32_t, label> notes;
};

TEST_SUITE(serde_flatbuffers_wire_adapter) {

TEST_CASE(adapted_types_round_trip_everywhere) {
    order input;
    input.id = ticket(42);
    input.name = label("answer");
    input.history = {ticket(1), ticket(2), ticket(3)};
    input.notes = {{7, label("seven")}, {9, label("nine")}};

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    order decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(decoded.id == input.id);
    EXPECT_TRUE(decoded.name == input.name);
    EXPECT_TRUE(decoded.history == input.history);
    EXPECT_TRUE(decoded.notes == input.notes);
}

TEST_CASE(proxy_reads_wire_representation) {
    order input;
    input.id = ticket(42);
    input.name = label("answer");

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<order>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());
    // Adapted fields surface as their wire representation.
    EXPECT_EQ(root[&order::id], 42U);
    EXPECT_EQ(root[&order::name], "answer");
}

};  // TEST_SUITE(serde_flatbuffers_wire_adapter)

}  // namespace

}  // namespace kota::codec

#endif
