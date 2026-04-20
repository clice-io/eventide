#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "kota/meta/struct.h"
#include "kota/codec/arena/encode.h"
#include "kota/codec/arena/traits.h"
#include "kota/codec/capnp/struct_layout.h"
#include "kota/codec/config.h"

namespace kota::codec::capnp {

// Cap'n Proto-shape arena backend.
//
// The wire format is *inspired by* Cap'n Proto: word-aligned (8 bytes),
// pointer-based indirection, composite-list layout for inline-struct
// vectors. It is NOT canonical capnp binary — canonical capnp is
// schema-driven, and the arena codec's slot_id is type-agnostic, so each
// struct embeds a small runtime slot table (like flatbuffers vtables).
//
// File layout (all fields little-endian):
//
//   header (16 bytes):
//     [0..8)   magic = "EVCPBUF\0"
//     [8..16)  u64 root_offset (byte offset to root struct from file start)
//
//   heap (everything else), 8-byte aligned:
//     struct = { u32 num_slots, u32 pad,
//                slot[0..num_slots] }
//     slot   = { u32 slot_id, u32 kind, u64 payload }
//         kind = 0 -> scalar; payload holds the value bits (caller
//                     knows the type from the template parameter, so no
//                     further type tag is needed).
//         kind = 1 -> pointer; payload holds an absolute byte offset from
//                     file start to the pointee.
//
//   string  = { u32 byte_len, bytes[byte_len], NUL byte, pad-to-8 }
//   bytes   = { u32 byte_len, bytes[byte_len], pad-to-8 }
//   scalar_vec       = { u32 stride, u32 count, count*stride bytes, pad-to-8 }
//   string_vec       = { u32 count, u32 pad, count * u64 offset }
//   inline_struct_vec (composite-list shape)
//                    = { u32 stride, u32 count, count*stride bytes, pad-to-8 }
//   table_vec        = { u32 count, u32 pad, count * u64 offset }
//
// Only pointer offsets are stored as absolute byte offsets from file
// start; no relative/far-pointer machinery is required because the
// serializer writes to a single contiguous buffer.

enum class object_error_code : std::uint8_t {
    none = 0,
    invalid_state,
    unsupported_type,
    type_mismatch,
    number_out_of_range,
    too_many_fields,
};

constexpr std::string_view error_message(object_error_code code) {
    switch(code) {
        case object_error_code::none: return "none";
        case object_error_code::invalid_state: return "invalid state";
        case object_error_code::unsupported_type: return "unsupported type";
        case object_error_code::type_mismatch: return "type mismatch";
        case object_error_code::number_out_of_range: return "number out of range";
        case object_error_code::too_many_fields: return "too many fields";
    }
    return "invalid state";
}

template <typename T>
using object_result_t = std::expected<T, object_error_code>;

namespace detail {

constexpr inline char buffer_magic[8] = {'E', 'V', 'C', 'P', 'B', 'U', 'F', '\0'};
constexpr inline std::size_t header_size = 16;
constexpr inline std::size_t word_size = 8;

constexpr inline std::uint32_t kind_scalar = 0;
constexpr inline std::uint32_t kind_pointer = 1;

struct slot_entry {
    std::uint32_t slot_id;
    std::uint32_t kind;
    std::uint64_t payload;
};

constexpr auto align_up(std::size_t n, std::size_t a) -> std::size_t {
    return (n + a - 1) & ~(a - 1);
}

// Map a slot index to an opaque slot id. We simply reuse the flatbuffers
// scheme (4 + i*2) so it's easy to cross-check in tests, but any strictly
// increasing, distinct mapping would work.
constexpr std::uint32_t first_field = 4;
constexpr std::uint32_t field_step = 2;

// True for "empty marker" reflectable types (std::monostate and similar):
// reflectable with zero fields and a trivial standard-layout representation.
// Regular reflectable structs with fields evaluate to false and take the
// pointer-indirected encode_table path (true capnp field semantics).
template <typename T>
constexpr bool is_empty_marker_v = []() consteval {
    if constexpr(meta::reflectable_class<T>) {
        return meta::field_count<T>() == 0 && std::is_trivial_v<T> && std::is_standard_layout_v<T>;
    } else {
        return false;
    }
}();

inline auto field_slot_id_of(std::size_t index) -> object_result_t<std::uint32_t> {
    constexpr auto max_slot = static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)());
    const auto raw =
        static_cast<std::size_t>(first_field) + index * static_cast<std::size_t>(field_step);
    if(raw > max_slot) {
        return std::unexpected(object_error_code::too_many_fields);
    }
    return static_cast<std::uint32_t>(raw);
}

inline auto variant_payload_slot_id_of(std::size_t index) -> object_result_t<std::uint32_t> {
    return field_slot_id_of(index + 1);
}

// Convert a typed scalar into the 64-bit payload we store in a slot. For
// floats we pun through the matching unsigned integer so NaN bit patterns
// survive round-trips; signed integers are sign-extended into the payload
// (the decode side re-narrows via static_cast<T>).
template <typename T>
inline auto scalar_to_payload(T value) -> std::uint64_t {
    if constexpr(std::is_same_v<T, bool>) {
        return value ? std::uint64_t{1} : std::uint64_t{0};
    } else if constexpr(std::is_same_v<T, float>) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return static_cast<std::uint64_t>(bits);
    } else if constexpr(std::is_same_v<T, double>) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    } else if constexpr(std::is_signed_v<T>) {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
    } else {
        return static_cast<std::uint64_t>(value);
    }
}

}  // namespace detail

template <typename Config = config::default_config>
class Serializer {
public:
    using config_type = Config;
    using error_type = object_error_code;

    // Slot id = opaque 32-bit field id. The arena codec derives it from a
    // field index via field_slot_id() / variant_*_slot_id().
    using slot_id = std::uint32_t;

    // All pointer refs are absolute byte offsets into the serialized
    // buffer. Distinct typedefs keep add_offset<RefT> strongly typed.
    using string_ref = std::uint64_t;
    using vector_ref = std::uint64_t;
    using table_ref = std::uint64_t;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    // Canonical Cap'n Proto never stores a multi-field struct inline in a
    // parent's field slot — struct fields always live in the pointer
    // section. Our backend honours that for real structs. The only
    // exception is zero-field reflectable *marker types* (e.g. the
    // std::monostate alternative used in std::variant<std::monostate, …>),
    // which the arena codec routes through the inline-struct path since
    // they carry no slot layout to spell out. Treating such markers as
    // tiny inline structs keeps the arena dispatch uniform.
    template <typename T>
    constexpr static bool can_inline_struct_field = detail::is_empty_marker_v<T>;

    // Elements of a list can be laid out inline via capnp's composite-list
    // pattern — shared element tag + contiguous stride data. Matches the
    // same POD / no-annotation predicate the flatbuffers backend uses.
    template <typename T>
    constexpr static bool can_inline_struct_element = capnp::can_inline_struct_v<T>;

    static auto field_slot_id(std::size_t index) -> result_t<slot_id> {
        return detail::field_slot_id_of(index);
    }

    static auto variant_tag_slot_id() -> slot_id {
        return detail::first_field;
    }

    static auto variant_payload_slot_id(std::size_t index) -> result_t<slot_id> {
        return detail::variant_payload_slot_id_of(index);
    }

    explicit Serializer(std::size_t initial_capacity = 1024) {
        buffer.reserve(initial_capacity);
        buffer.resize(detail::header_size, 0);
        std::memcpy(buffer.data(), detail::buffer_magic, sizeof(detail::buffer_magic));
        // root_offset is written during finish().
    }

    // === TableBuilder =====================================================
    //
    // Collects slot entries in memory. finalize() emits the struct body
    // (header + slot array) at the current buffer tail and returns its
    // byte offset.
    //
    // Unlike the flatbuffers backend, writes can proceed while a table is
    // open because our buffer is written strictly forward — allocating a
    // string/vector while a TableBuilder is "open" is fine; finalize()
    // just appends the struct after the nested data.
    class TableBuilder {
    public:
        explicit TableBuilder(Serializer& owner) : owner(&owner) {}

        template <typename T>
        auto add_scalar(slot_id sid, T value) -> void {
            slots.push_back({sid, detail::kind_scalar, detail::scalar_to_payload<T>(value)});
        }

        template <typename RefT>
        auto add_offset(slot_id sid, RefT ref) -> void {
            slots.push_back({sid, detail::kind_pointer, static_cast<std::uint64_t>(ref)});
        }

        // Never invoked in practice (can_inline_struct_field = false), but
        // required by the arena concept's textual shape. Fall back to a
        // table_ref-style pointer containing a tiny "struct-sized" blob.
        template <typename T>
        auto add_inline_struct(slot_id sid, const T& value) -> void {
            const auto offset = owner->append_pod(value);
            slots.push_back({sid, detail::kind_pointer, offset});
        }

        auto finalize() -> result_t<table_ref> {
            owner->pad_to_word();
            const auto offset = static_cast<std::uint64_t>(owner->buffer.size());

            const auto num = static_cast<std::uint32_t>(slots.size());
            owner->write_u32(num);
            owner->write_u32(0);  // pad
            for(const auto& s: slots) {
                owner->write_u32(s.slot_id);
                owner->write_u32(s.kind);
                owner->write_u64(s.payload);
            }
            slots.clear();
            return static_cast<table_ref>(offset);
        }

    private:
        Serializer* owner;
        std::vector<detail::slot_entry> slots;
    };

    auto start_table() -> TableBuilder {
        return TableBuilder(*this);
    }

    // === Allocation primitives ===========================================

    auto alloc_string(std::string_view text) -> result_t<string_ref> {
        pad_to_word();
        const auto offset = static_cast<std::uint64_t>(buffer.size());
        write_u32(static_cast<std::uint32_t>(text.size()));
        // length is u32; body starts on 4-byte boundary but we realign
        // after the NUL to the next 8-byte boundary.
        write_u32(0);  // pad
        append_raw(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
        buffer.push_back(0);  // NUL terminator (capnp-style)
        pad_to_word();
        return static_cast<string_ref>(offset);
    }

    auto alloc_bytes(std::span<const std::byte> bytes) -> result_t<vector_ref> {
        pad_to_word();
        const auto offset = static_cast<std::uint64_t>(buffer.size());
        write_u32(static_cast<std::uint32_t>(bytes.size()));
        write_u32(0);
        append_raw(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
        pad_to_word();
        return static_cast<vector_ref>(offset);
    }

    template <typename T>
    auto alloc_scalar_vector(std::span<const T> elements) -> result_t<vector_ref> {
        static_assert(std::is_trivially_copyable_v<T>,
                      "capnp scalar_vector element must be trivially copyable");
        pad_to_word();
        const auto offset = static_cast<std::uint64_t>(buffer.size());
        write_u32(static_cast<std::uint32_t>(sizeof(T)));
        write_u32(static_cast<std::uint32_t>(elements.size()));
        append_raw(reinterpret_cast<const std::uint8_t*>(elements.data()),
                   elements.size() * sizeof(T));
        pad_to_word();
        return static_cast<vector_ref>(offset);
    }

    auto alloc_string_vector(std::span<const string_ref> elements) -> result_t<vector_ref> {
        pad_to_word();
        const auto offset = static_cast<std::uint64_t>(buffer.size());
        write_u32(static_cast<std::uint32_t>(elements.size()));
        write_u32(0);
        for(const auto e: elements) {
            write_u64(static_cast<std::uint64_t>(e));
        }
        // Already 8-aligned (u64 array).
        return static_cast<vector_ref>(offset);
    }

    template <typename T>
    auto alloc_inline_struct_vector(std::span<const T> elements) -> result_t<vector_ref> {
        static_assert(std::is_trivially_copyable_v<T>,
                      "capnp inline_struct_vector element must be trivially copyable");
        pad_to_word();
        const auto offset = static_cast<std::uint64_t>(buffer.size());
        write_u32(static_cast<std::uint32_t>(sizeof(T)));
        write_u32(static_cast<std::uint32_t>(elements.size()));
        append_raw(reinterpret_cast<const std::uint8_t*>(elements.data()),
                   elements.size() * sizeof(T));
        pad_to_word();
        return static_cast<vector_ref>(offset);
    }

    auto alloc_table_vector(std::span<const table_ref> elements) -> result_t<vector_ref> {
        pad_to_word();
        const auto offset = static_cast<std::uint64_t>(buffer.size());
        write_u32(static_cast<std::uint32_t>(elements.size()));
        write_u32(0);
        for(const auto e: elements) {
            write_u64(static_cast<std::uint64_t>(e));
        }
        return static_cast<vector_ref>(offset);
    }

    auto finish(table_ref root) -> result_t<void> {
        const auto root_offset = static_cast<std::uint64_t>(root);
        std::memcpy(buffer.data() + 8, &root_offset, sizeof(root_offset));
        return {};
    }

    auto bytes() -> std::vector<std::uint8_t> {
        return buffer;
    }

    // === Top-level entry (public) ========================================
    template <typename T>
    auto bytes(const T& value) -> result_t<std::vector<std::uint8_t>> {
        buffer.clear();
        buffer.resize(detail::header_size, 0);
        std::memcpy(buffer.data(), detail::buffer_magic, sizeof(detail::buffer_magic));

        KOTA_EXPECTED_TRY_V(auto root, (arena::encode_root<Config>(*this, value)));
        KOTA_EXPECTED_TRY(finish(root));
        return bytes();
    }

private:
    template <typename T>
    auto append_pod(const T& value) -> std::uint64_t {
        static_assert(std::is_trivially_copyable_v<T>, "inline struct must be POD");
        pad_to_word();
        const auto offset = static_cast<std::uint64_t>(buffer.size());
        const auto* raw = reinterpret_cast<const std::uint8_t*>(std::addressof(value));
        append_raw(raw, sizeof(T));
        pad_to_word();
        return offset;
    }

    auto append_raw(const std::uint8_t* data, std::size_t n) -> void {
        buffer.insert(buffer.end(), data, data + n);
    }

    auto write_u32(std::uint32_t v) -> void {
        std::uint8_t raw[4];
        std::memcpy(raw, &v, 4);
        buffer.insert(buffer.end(), raw, raw + 4);
    }

    auto write_u64(std::uint64_t v) -> void {
        std::uint8_t raw[8];
        std::memcpy(raw, &v, 8);
        buffer.insert(buffer.end(), raw, raw + 8);
    }

    auto pad_to_word() -> void {
        const auto aligned = detail::align_up(buffer.size(), detail::word_size);
        buffer.resize(aligned, 0);
    }

    std::vector<std::uint8_t> buffer;
};

template <typename Config = config::default_config, typename T>
auto to_capnp(const T& value, std::optional<std::size_t> initial_capacity = std::nullopt)
    -> object_result_t<std::vector<std::uint8_t>> {
    Serializer<Config> serializer(initial_capacity.value_or(1024));
    return serializer.bytes(value);
}

}  // namespace kota::codec::capnp
