#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "kota/support/expected_try.h"
#include "kota/meta/struct.h"
#include "kota/codec/arena/decode.h"
#include "kota/codec/arena/traits.h"
#include "kota/codec/capnp/serializer.h"
#include "kota/codec/capnp/struct_layout.h"
#include "kota/codec/config.h"

namespace kota::codec::capnp {

namespace detail {

// Read a little-endian integer out of a byte buffer with bounds checking
// baked in. Returns nullopt when the read would fall off the end.
template <typename T>
inline auto read_at(std::span<const std::uint8_t> buffer, std::uint64_t offset)
    -> std::optional<T> {
    static_assert(std::is_trivially_copyable_v<T>);
    if(offset > buffer.size() || buffer.size() - offset < sizeof(T)) {
        return std::nullopt;
    }
    T value{};
    std::memcpy(&value, buffer.data() + offset, sizeof(T));
    return value;
}

// Convert a 64-bit payload back into a typed scalar. Mirrors
// `scalar_to_payload` on the encode side.
template <typename T>
inline auto payload_to_scalar(std::uint64_t payload) -> T {
    if constexpr(std::is_same_v<T, bool>) {
        return payload != 0;
    } else if constexpr(std::is_same_v<T, float>) {
        std::uint32_t bits = static_cast<std::uint32_t>(payload);
        float value = 0.0F;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    } else if constexpr(std::is_same_v<T, double>) {
        double value = 0.0;
        std::memcpy(&value, &payload, sizeof(value));
        return value;
    } else if constexpr(std::is_signed_v<T>) {
        return static_cast<T>(static_cast<std::int64_t>(payload));
    } else {
        return static_cast<T>(payload);
    }
}

// View wrappers returned by Deserializer::get_*_vector.

template <typename T>
class scalar_vector_view {
public:
    scalar_vector_view() = default;

    scalar_vector_view(const std::uint8_t* data, std::size_t count) :
        data_ptr(data), element_count(count) {}

    auto size() const -> std::size_t {
        return element_count;
    }

    auto operator[](std::size_t index) const -> T {
        T value{};
        std::memcpy(&value, data_ptr + index * sizeof(T), sizeof(T));
        return value;
    }

private:
    const std::uint8_t* data_ptr = nullptr;
    std::size_t element_count = 0;
};

class string_vector_view {
public:
    string_vector_view() = default;

    string_vector_view(std::span<const std::uint8_t> buffer,
                       const std::uint8_t* offsets,
                       std::size_t count) :
        file_buffer(buffer), offsets_ptr(offsets), element_count(count) {}

    auto size() const -> std::size_t {
        return element_count;
    }

    auto operator[](std::size_t index) const -> std::string_view {
        std::uint64_t offset = 0;
        std::memcpy(&offset, offsets_ptr + index * sizeof(std::uint64_t), sizeof(offset));
        // String layout: u32 len, u32 pad, bytes[len], NUL, pad8.
        auto len_opt = read_at<std::uint32_t>(file_buffer, offset);
        if(!len_opt) {
            return {};
        }
        const auto payload_offset = offset + 8;  // skip u32 len + u32 pad
        if(payload_offset > file_buffer.size() || file_buffer.size() - payload_offset < *len_opt) {
            return {};
        }
        return std::string_view(reinterpret_cast<const char*>(file_buffer.data() + payload_offset),
                                *len_opt);
    }

private:
    std::span<const std::uint8_t> file_buffer;
    const std::uint8_t* offsets_ptr = nullptr;
    std::size_t element_count = 0;
};

template <typename T>
class inline_struct_vector_view {
public:
    inline_struct_vector_view() = default;

    inline_struct_vector_view(const std::uint8_t* data, std::size_t count, std::size_t stride) :
        data_ptr(data), element_count(count), element_stride(stride) {}

    auto size() const -> std::size_t {
        return element_count;
    }

    auto operator[](std::size_t index) const -> const T& {
        return *reinterpret_cast<const T*>(data_ptr + index * element_stride);
    }

private:
    const std::uint8_t* data_ptr = nullptr;
    std::size_t element_count = 0;
    std::size_t element_stride = 0;
};

template <typename TableView>
class table_vector_view {
public:
    table_vector_view() = default;

    table_vector_view(std::span<const std::uint8_t> buffer,
                      const std::uint8_t* offsets,
                      std::size_t count) :
        file_buffer(buffer), offsets_ptr(offsets), element_count(count) {}

    auto size() const -> std::size_t {
        return element_count;
    }

    auto operator[](std::size_t index) const -> TableView {
        std::uint64_t offset = 0;
        std::memcpy(&offset, offsets_ptr + index * sizeof(std::uint64_t), sizeof(offset));
        return TableView::from_offset(file_buffer, offset);
    }

private:
    std::span<const std::uint8_t> file_buffer;
    const std::uint8_t* offsets_ptr = nullptr;
    std::size_t element_count = 0;
};

}  // namespace detail

// Arena-codec capnp deserializer. Thin wrapper around a byte buffer
// produced by the matching Serializer.
template <typename Config = config::default_config>
class Deserializer {
public:
    using config_type = Config;
    using error_type = object_error_code;
    using slot_id = std::uint32_t;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    // Mirror of the serializer-side predicates. See serializer.h for the
    // rationale (empty marker types inline; real structs pointer-indirect).
    template <typename T>
    constexpr static bool can_inline_struct_field = detail::is_empty_marker_v<T>;

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

    // === Table view ======================================================

    class TableView {
    public:
        TableView() = default;

        TableView(std::span<const std::uint8_t> buffer, std::uint64_t body_offset) :
            file_buffer(buffer), offset(body_offset), is_valid(true) {
            // Parse the struct header lazily; mark invalid on any
            // inconsistency so callers can short-circuit.
            auto num_opt = detail::read_at<std::uint32_t>(file_buffer, offset);
            if(!num_opt) {
                is_valid = false;
                return;
            }
            num_slots = *num_opt;
            // slots start at offset + 8 (skip u32 num_slots + u32 pad).
            const auto slots_start = offset + 8;
            const auto slots_bytes =
                static_cast<std::uint64_t>(num_slots) * sizeof(detail::slot_entry);
            if(slots_start > file_buffer.size() || file_buffer.size() - slots_start < slots_bytes) {
                is_valid = false;
            }
        }

        // Factory used by list/vector views so they can validate before
        // constructing a TableView.
        static auto from_offset(std::span<const std::uint8_t> buffer, std::uint64_t body_offset)
            -> TableView {
            return TableView(buffer, body_offset);
        }

        auto valid() const -> bool {
            return is_valid;
        }

        auto has(slot_id sid) const -> bool {
            return find_slot(sid) != nullptr;
        }

        auto any_field_present() const -> bool {
            return is_valid && num_slots > 0;
        }

        template <typename T>
        auto get_scalar(slot_id sid) const -> T {
            const auto* s = find_slot(sid);
            if(s == nullptr || s->kind != detail::kind_scalar) {
                return T{};
            }
            return detail::payload_to_scalar<T>(s->payload);
        }

        // Internal: retrieve the pointer offset for a pointer-kind slot.
        auto get_pointer_offset(slot_id sid) const -> std::optional<std::uint64_t> {
            const auto* s = find_slot(sid);
            if(s == nullptr || s->kind != detail::kind_pointer) {
                return std::nullopt;
            }
            return s->payload;
        }

        auto file_bytes() const -> std::span<const std::uint8_t> {
            return file_buffer;
        }

    private:
        auto find_slot(slot_id sid) const -> const detail::slot_entry* {
            if(!is_valid) {
                return nullptr;
            }
            const auto* base = file_buffer.data() + offset + 8;
            for(std::uint32_t i = 0; i < num_slots; ++i) {
                detail::slot_entry entry{};
                std::memcpy(&entry, base + i * sizeof(detail::slot_entry), sizeof(entry));
                if(entry.slot_id == sid) {
                    // Return a pointer into the buffer; entries are POD
                    // and trivially copyable, so reinterpreting the raw
                    // bytes as a slot_entry is safe on all supported
                    // targets (little-endian with 4-byte alignment).
                    return reinterpret_cast<const detail::slot_entry*>(
                        base + i * sizeof(detail::slot_entry));
                }
            }
            return nullptr;
        }

        std::span<const std::uint8_t> file_buffer;
        std::uint64_t offset = 0;
        std::uint32_t num_slots = 0;
        bool is_valid = false;
    };

    // === Construction ====================================================

    explicit Deserializer(std::span<const std::uint8_t> raw) : file_buffer(raw.data(), raw.size()) {
        initialize();
    }

    explicit Deserializer(std::span<const std::byte> raw) :
        file_buffer(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size()) {
        initialize();
    }

    explicit Deserializer(const std::vector<std::uint8_t>& raw) :
        Deserializer(std::span<const std::uint8_t>(raw.data(), raw.size())) {}

    auto valid() const -> bool {
        return is_valid;
    }

    auto error() const -> error_type {
        return is_valid ? object_error_code::none : last_error;
    }

    auto root_view() const -> TableView {
        if(!is_valid) {
            return TableView{};
        }
        return TableView(file_buffer, root_offset);
    }

    // === Typed accessors =================================================

    auto get_string(TableView view, slot_id sid) const -> result_t<std::string_view> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset = *offset_opt;
        const auto len_opt = detail::read_at<std::uint32_t>(file_buffer, offset);
        if(!len_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto payload_offset = offset + 8;  // u32 len + u32 pad
        if(payload_offset > file_buffer.size() || file_buffer.size() - payload_offset < *len_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        return std::string_view(reinterpret_cast<const char*>(file_buffer.data() + payload_offset),
                                *len_opt);
    }

    auto get_bytes(TableView view, slot_id sid) const -> result_t<std::span<const std::byte>> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset = *offset_opt;
        const auto len_opt = detail::read_at<std::uint32_t>(file_buffer, offset);
        if(!len_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto payload_offset = offset + 8;
        if(payload_offset > file_buffer.size() || file_buffer.size() - payload_offset < *len_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        return std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(file_buffer.data() + payload_offset),
            *len_opt);
    }

    template <typename T>
    auto get_scalar_vector(TableView view, slot_id sid) const
        -> result_t<detail::scalar_vector_view<T>> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset = *offset_opt;
        const auto stride_opt = detail::read_at<std::uint32_t>(file_buffer, offset);
        const auto count_opt = detail::read_at<std::uint32_t>(file_buffer, offset + 4);
        if(!stride_opt || !count_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        if(*stride_opt != sizeof(T)) {
            return std::unexpected(object_error_code::type_mismatch);
        }
        const auto payload_offset = offset + 8;
        const auto payload_bytes =
            static_cast<std::uint64_t>(*count_opt) * static_cast<std::uint64_t>(*stride_opt);
        if(payload_offset > file_buffer.size() ||
           file_buffer.size() - payload_offset < payload_bytes) {
            return std::unexpected(object_error_code::invalid_state);
        }
        return detail::scalar_vector_view<T>(file_buffer.data() + payload_offset, *count_opt);
    }

    auto get_string_vector(TableView view, slot_id sid) const
        -> result_t<detail::string_vector_view> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset = *offset_opt;
        const auto count_opt = detail::read_at<std::uint32_t>(file_buffer, offset);
        if(!count_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto payload_offset = offset + 8;
        const auto payload_bytes = static_cast<std::uint64_t>(*count_opt) * sizeof(std::uint64_t);
        if(payload_offset > file_buffer.size() ||
           file_buffer.size() - payload_offset < payload_bytes) {
            return std::unexpected(object_error_code::invalid_state);
        }
        return detail::string_vector_view(file_buffer,
                                          file_buffer.data() + payload_offset,
                                          *count_opt);
    }

    template <typename T>
    auto get_inline_struct(TableView view, slot_id sid) const -> result_t<T> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset = *offset_opt;
        if(offset > file_buffer.size() || file_buffer.size() - offset < sizeof(T)) {
            return std::unexpected(object_error_code::invalid_state);
        }
        T value{};
        std::memcpy(&value, file_buffer.data() + offset, sizeof(T));
        return value;
    }

    template <typename T>
    auto get_inline_struct_vector(TableView view, slot_id sid) const
        -> result_t<detail::inline_struct_vector_view<T>> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset = *offset_opt;
        const auto stride_opt = detail::read_at<std::uint32_t>(file_buffer, offset);
        const auto count_opt = detail::read_at<std::uint32_t>(file_buffer, offset + 4);
        if(!stride_opt || !count_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        if(*stride_opt != sizeof(T)) {
            return std::unexpected(object_error_code::type_mismatch);
        }
        const auto payload_offset = offset + 8;
        const auto payload_bytes =
            static_cast<std::uint64_t>(*count_opt) * static_cast<std::uint64_t>(*stride_opt);
        if(payload_offset > file_buffer.size() ||
           file_buffer.size() - payload_offset < payload_bytes) {
            return std::unexpected(object_error_code::invalid_state);
        }
        return detail::inline_struct_vector_view<T>(file_buffer.data() + payload_offset,
                                                    *count_opt,
                                                    *stride_opt);
    }

    auto get_table(TableView view, slot_id sid) const -> result_t<TableView> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        TableView nested(file_buffer, *offset_opt);
        if(!nested.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        return nested;
    }

    auto get_table_vector(TableView view, slot_id sid) const
        -> result_t<detail::table_vector_view<TableView>> {
        if(!view.valid()) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset_opt = view.get_pointer_offset(sid);
        if(!offset_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto offset = *offset_opt;
        const auto count_opt = detail::read_at<std::uint32_t>(file_buffer, offset);
        if(!count_opt) {
            return std::unexpected(object_error_code::invalid_state);
        }
        const auto payload_offset = offset + 8;
        const auto payload_bytes = static_cast<std::uint64_t>(*count_opt) * sizeof(std::uint64_t);
        if(payload_offset > file_buffer.size() ||
           file_buffer.size() - payload_offset < payload_bytes) {
            return std::unexpected(object_error_code::invalid_state);
        }
        return detail::table_vector_view<TableView>(file_buffer,
                                                    file_buffer.data() + payload_offset,
                                                    *count_opt);
    }

    // === Top-level entry =================================================

    template <typename T>
    auto deserialize(T& value) const -> result_t<void> {
        if(!is_valid) {
            return std::unexpected(last_error);
        }
        return arena::decode_root<Config>(*this, value);
    }

private:
    auto initialize() -> void {
        if(file_buffer.size() < detail::header_size) {
            set_invalid(object_error_code::invalid_state);
            return;
        }
        if(std::memcmp(file_buffer.data(), detail::buffer_magic, sizeof(detail::buffer_magic)) !=
           0) {
            set_invalid(object_error_code::invalid_state);
            return;
        }
        std::uint64_t root = 0;
        std::memcpy(&root, file_buffer.data() + 8, sizeof(root));
        if(root < detail::header_size || root > file_buffer.size()) {
            set_invalid(object_error_code::invalid_state);
            return;
        }
        root_offset = root;

        // Pre-validate the root struct header.
        TableView probe(file_buffer, root_offset);
        if(!probe.valid()) {
            set_invalid(object_error_code::invalid_state);
            return;
        }
    }

    auto set_invalid(error_type err) -> void {
        is_valid = false;
        last_error = err;
    }

    std::span<const std::uint8_t> file_buffer;
    std::uint64_t root_offset = 0;
    bool is_valid = true;
    error_type last_error = object_error_code::none;
};

template <typename Config = config::default_config, typename T>
auto from_capnp(std::span<const std::uint8_t> bytes, T& value)
    -> std::expected<void, object_error_code> {
    Deserializer<Config> d(bytes);
    if(!d.valid()) {
        return std::unexpected(d.error());
    }
    KOTA_EXPECTED_TRY(d.deserialize(value));
    return {};
}

template <typename Config = config::default_config, typename T>
auto from_capnp(std::span<const std::byte> bytes, T& value)
    -> std::expected<void, object_error_code> {
    Deserializer<Config> d(bytes);
    if(!d.valid()) {
        return std::unexpected(d.error());
    }
    KOTA_EXPECTED_TRY(d.deserialize(value));
    return {};
}

template <typename Config = config::default_config, typename T>
auto from_capnp(const std::vector<std::uint8_t>& bytes, T& value)
    -> std::expected<void, object_error_code> {
    return from_capnp<Config>(std::span<const std::uint8_t>(bytes.data(), bytes.size()), value);
}

template <typename T, typename Config = config::default_config>
    requires std::default_initializable<T>
auto from_capnp(std::span<const std::uint8_t> bytes) -> std::expected<T, object_error_code> {
    T value{};
    KOTA_EXPECTED_TRY(from_capnp<Config>(bytes, value));
    return value;
}

template <typename T, typename Config = config::default_config>
    requires std::default_initializable<T>
auto from_capnp(const std::vector<std::uint8_t>& bytes) -> std::expected<T, object_error_code> {
    return from_capnp<T, Config>(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
}

}  // namespace kota::codec::capnp
