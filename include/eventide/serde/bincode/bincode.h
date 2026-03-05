#pragma once

#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "eventide/common/ranges.h"
#include "eventide/serde/detail/narrow.h"
#include "eventide/serde/serde.h"

namespace eventide::serde::bincode {

enum class error_kind : std::uint8_t {
    ok = 0,
    invalid_state,
    unexpected_eof,
    invalid_type,
    number_out_of_range,
    trailing_bytes,
    invalid_variant_index,
    unsupported_operation,
};

constexpr std::string_view error_message(error_kind error) {
    switch(error) {
        case error_kind::ok: return "ok";
        case error_kind::invalid_state: return "invalid_state";
        case error_kind::unexpected_eof: return "unexpected_eof";
        case error_kind::invalid_type: return "invalid_type";
        case error_kind::number_out_of_range: return "number_out_of_range";
        case error_kind::trailing_bytes: return "trailing_bytes";
        case error_kind::invalid_variant_index: return "invalid_variant_index";
        case error_kind::unsupported_operation: return "unsupported_operation";
    }

    return "invalid_state";
}

class Serializer {
public:
    using value_type = void;
    using error_type = error_kind;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    class SerializeElements {
    public:
        SerializeElements(Serializer& serializer, std::size_t expected_count) noexcept :
            serializer(serializer), expected_count(expected_count) {}

        template <typename T>
        status_t serialize_element(const T& value) {
            if(written_count >= expected_count) {
                return serializer.mark_invalid(error_type::invalid_state);
            }

            auto status = serde::serialize(serializer, value);
            if(!status) {
                return std::unexpected(status.error());
            }

            ++written_count;
            return {};
        }

        template <typename K, typename V>
        status_t serialize_entry(const K& key, const V& value) {
            if(written_count >= expected_count) {
                return serializer.mark_invalid(error_type::invalid_state);
            }

            auto key_status = serde::serialize(serializer, key);
            if(!key_status) {
                return std::unexpected(key_status.error());
            }

            auto value_status = serde::serialize(serializer, value);
            if(!value_status) {
                return std::unexpected(value_status.error());
            }

            ++written_count;
            return {};
        }

        template <typename T>
        status_t serialize_field(std::string_view /*key*/, const T& value) {
            return serialize_element(value);
        }

        result_t<value_type> end() {
            if(written_count != expected_count) {
                return serializer.mark_invalid(error_type::invalid_state);
            }
            return {};
        }

    private:
        Serializer& serializer;
        std::size_t expected_count = 0;
        std::size_t written_count = 0;
    };

    using SerializeSeq = SerializeElements;
    using SerializeTuple = SerializeElements;
    using SerializeMap = SerializeElements;
    using SerializeStruct = SerializeElements;

    Serializer() = default;

    explicit Serializer(std::size_t reserve_bytes) {
        bytes_buffer.reserve(reserve_bytes);
    }

    [[nodiscard]] bool valid() const noexcept {
        return is_valid;
    }

    [[nodiscard]] error_type error() const noexcept {
        return current_error();
    }

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return std::span<const std::byte>(bytes_buffer.data(), bytes_buffer.size());
    }

    auto take_bytes() -> std::vector<std::byte> {
        return std::move(bytes_buffer);
    }

    result_t<value_type> serialize_null() {
        return write_u8(0);
    }

    template <typename T>
    result_t<value_type> serialize_some(const T& value) {
        auto status = write_u8(1);
        if(!status) {
            return std::unexpected(status.error());
        }
        return serde::serialize(*this, value);
    }

    result_t<value_type> serialize_bool(bool value) {
        return write_u8(value ? 1 : 0);
    }

    result_t<value_type> serialize_int(std::int64_t value) {
        return write_integral(value);
    }

    result_t<value_type> serialize_uint(std::uint64_t value) {
        return write_integral(value);
    }

    result_t<value_type> serialize_float(double value) {
        const auto bits = std::bit_cast<std::uint64_t>(value);
        return write_integral(bits);
    }

    result_t<value_type> serialize_char(char value) {
        return write_u8(static_cast<std::uint8_t>(value));
    }

    result_t<value_type> serialize_str(std::string_view value) {
        auto length_status = write_length(value.size());
        if(!length_status) {
            return std::unexpected(length_status.error());
        }

        if(!is_valid) {
            return std::unexpected(current_error());
        }

        if(value.empty()) {
            return {};
        }

        bytes_buffer.insert(bytes_buffer.end(),
                            reinterpret_cast<const std::byte*>(value.data()),
                            reinterpret_cast<const std::byte*>(value.data() + value.size()));
        return {};
    }

    result_t<value_type> serialize_bytes(std::span<const std::byte> value) {
        auto length_status = write_length(value.size());
        if(!length_status) {
            return std::unexpected(length_status.error());
        }

        if(!is_valid) {
            return std::unexpected(current_error());
        }

        bytes_buffer.insert(bytes_buffer.end(), value.begin(), value.end());
        return {};
    }

    template <typename... Ts>
    result_t<value_type> serialize_variant(const std::variant<Ts...>& value) {
        const auto variant_index = value.index();
        if(variant_index > (std::numeric_limits<std::uint32_t>::max)()) {
            return mark_invalid(error_type::invalid_variant_index);
        }

        auto index_status = write_integral(static_cast<std::uint32_t>(variant_index));
        if(!index_status) {
            return std::unexpected(index_status.error());
        }

        std::expected<void, error_type> payload_status{};
        std::visit(
            [&](const auto& item) {
                using item_t = std::remove_cvref_t<decltype(item)>;
                if constexpr(std::same_as<item_t, std::monostate>) {
                    payload_status = {};
                } else {
                    auto serialized = serde::serialize(*this, item);
                    if(!serialized) {
                        payload_status = std::unexpected(serialized.error());
                    } else {
                        payload_status = {};
                    }
                }
            },
            value);

        if(!payload_status) {
            return std::unexpected(payload_status.error());
        }
        return {};
    }

    result_t<SerializeSeq> serialize_seq(std::optional<std::size_t> len) {
        if(!len.has_value()) {
            return std::unexpected(error_type::invalid_state);
        }

        auto length_status = write_length(*len);
        if(!length_status) {
            return std::unexpected(length_status.error());
        }
        return SerializeSeq(*this, *len);
    }

    result_t<SerializeTuple> serialize_tuple(std::size_t len) {
        return SerializeTuple(*this, len);
    }

    result_t<SerializeMap> serialize_map(std::optional<std::size_t> len) {
        if(!len.has_value()) {
            return std::unexpected(error_type::invalid_state);
        }

        auto length_status = write_length(*len);
        if(!length_status) {
            return std::unexpected(length_status.error());
        }
        return SerializeMap(*this, *len);
    }

    result_t<SerializeStruct> serialize_struct(std::string_view /*name*/, std::size_t len) {
        return SerializeStruct(*this, len);
    }

private:
    template <typename T>
        requires std::integral<T>
    status_t write_integral(T value) {
        if(!is_valid) {
            return std::unexpected(current_error());
        }

        using unsigned_t = std::make_unsigned_t<T>;
        unsigned_t raw = static_cast<unsigned_t>(value);
        for(std::size_t i = 0; i < sizeof(unsigned_t); ++i) {
            const auto byte = static_cast<std::uint8_t>((raw >> (i * 8)) & 0xFFU);
            bytes_buffer.push_back(static_cast<std::byte>(byte));
        }
        return {};
    }

    status_t write_u8(std::uint8_t value) {
        return write_integral(value);
    }

    status_t write_length(std::size_t len) {
        if(static_cast<unsigned long long>(len) >
           static_cast<unsigned long long>((std::numeric_limits<std::uint64_t>::max)())) {
            return mark_invalid(error_type::invalid_state);
        }
        return write_integral(static_cast<std::uint64_t>(len));
    }

    status_t mark_invalid(error_type error) {
        is_valid = false;
        last_error = error;
        return std::unexpected(error);
    }

    error_type current_error() const {
        return is_valid ? error_type::ok : last_error;
    }

private:
    std::vector<std::byte> bytes_buffer;
    bool is_valid = true;
    error_type last_error = error_type::ok;
};

class Deserializer {
public:
    using error_type = error_kind;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    using status_t = result_t<void>;

    class DeserializeSeq {
    public:
        DeserializeSeq(Deserializer& deserializer, std::size_t expected_count) noexcept :
            deserializer(deserializer), expected_count(expected_count) {}

        result_t<bool> has_next() {
            if(!deserializer.is_valid) {
                return std::unexpected(deserializer.current_error());
            }
            return read_count < expected_count;
        }

        template <typename T>
        status_t deserialize_element(T& value) {
            auto has_next_value = has_next();
            if(!has_next_value) {
                return std::unexpected(has_next_value.error());
            }
            if(!*has_next_value) {
                return deserializer.mark_invalid(error_type::invalid_state);
            }

            auto status = serde::deserialize(deserializer, value);
            if(!status) {
                return std::unexpected(status.error());
            }

            ++read_count;
            return {};
        }

        status_t skip_element() {
            return deserializer.mark_invalid(error_type::unsupported_operation);
        }

        status_t end() {
            if(!deserializer.is_valid) {
                return std::unexpected(deserializer.current_error());
            }
            if(read_count != expected_count) {
                return deserializer.mark_invalid(error_type::invalid_state);
            }
            return {};
        }

    private:
        Deserializer& deserializer;
        std::size_t expected_count = 0;
        std::size_t read_count = 0;
    };

    class DeserializeTuple {
    public:
        DeserializeTuple(Deserializer& deserializer, std::size_t expected_count) noexcept :
            deserializer(deserializer), expected_count(expected_count) {}

        template <typename T>
        status_t deserialize_element(T& value) {
            if(!deserializer.is_valid) {
                return std::unexpected(deserializer.current_error());
            }
            if(read_count >= expected_count) {
                return deserializer.mark_invalid(error_type::invalid_state);
            }

            auto status = serde::deserialize(deserializer, value);
            if(!status) {
                return std::unexpected(status.error());
            }

            ++read_count;
            return {};
        }

        status_t skip_element() {
            return deserializer.mark_invalid(error_type::unsupported_operation);
        }

        status_t end() {
            if(!deserializer.is_valid) {
                return std::unexpected(deserializer.current_error());
            }
            if(read_count != expected_count) {
                return deserializer.mark_invalid(error_type::invalid_state);
            }
            return {};
        }

    private:
        Deserializer& deserializer;
        std::size_t expected_count = 0;
        std::size_t read_count = 0;
    };

    class DeserializeUnsupported {
    public:
        explicit DeserializeUnsupported(Deserializer& deserializer) noexcept :
            deserializer(deserializer) {}

        result_t<std::optional<std::string_view>> next_key() {
            return std::unexpected(error_type::unsupported_operation);
        }

        status_t invalid_key(std::string_view /*key_name*/) {
            return deserializer.mark_invalid(error_type::unsupported_operation);
        }

        template <typename T>
        status_t deserialize_value(T& /*value*/) {
            return deserializer.mark_invalid(error_type::unsupported_operation);
        }

        status_t skip_value() {
            return deserializer.mark_invalid(error_type::unsupported_operation);
        }

        status_t end() {
            return deserializer.mark_invalid(error_type::unsupported_operation);
        }

    private:
        Deserializer& deserializer;
    };

    using DeserializeMap = DeserializeUnsupported;
    using DeserializeStruct = DeserializeUnsupported;

    explicit Deserializer(std::span<const std::byte> bytes) : bytes(bytes) {}

    explicit Deserializer(std::span<const std::uint8_t> bytes) :
        bytes(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()) {}

    explicit Deserializer(const std::vector<std::byte>& bytes) :
        Deserializer(std::span<const std::byte>(bytes.data(), bytes.size())) {}

    explicit Deserializer(const std::vector<std::uint8_t>& bytes) :
        Deserializer(std::span<const std::uint8_t>(bytes.data(), bytes.size())) {}

    [[nodiscard]] bool valid() const noexcept {
        return is_valid;
    }

    [[nodiscard]] error_type error() const noexcept {
        return current_error();
    }

    status_t finish() {
        if(!is_valid) {
            return std::unexpected(current_error());
        }
        if(offset != bytes.size()) {
            return mark_invalid(error_type::trailing_bytes);
        }
        return {};
    }

    status_t deserialize_bool(bool& value) {
        auto parsed = read_u8();
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        if(*parsed > 1U) {
            return mark_invalid(error_type::invalid_type);
        }
        value = *parsed == 1U;
        return {};
    }

    template <serde::int_like T>
    status_t deserialize_int(T& value) {
        auto parsed = read_integral<std::int64_t>();
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        auto narrowed = serde::detail::narrow_int<T>(*parsed, error_type::number_out_of_range);
        if(!narrowed) {
            return mark_invalid(narrowed.error());
        }
        value = *narrowed;
        return {};
    }

    template <serde::uint_like T>
    status_t deserialize_uint(T& value) {
        auto parsed = read_integral<std::uint64_t>();
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        auto narrowed = serde::detail::narrow_uint<T>(*parsed, error_type::number_out_of_range);
        if(!narrowed) {
            return mark_invalid(narrowed.error());
        }
        value = *narrowed;
        return {};
    }

    template <serde::floating_like T>
    status_t deserialize_float(T& value) {
        auto raw = read_integral<std::uint64_t>();
        if(!raw) {
            return std::unexpected(raw.error());
        }

        const double parsed = std::bit_cast<double>(*raw);
        auto narrowed = serde::detail::narrow_float<T>(parsed, error_type::number_out_of_range);
        if(!narrowed) {
            return mark_invalid(narrowed.error());
        }
        value = *narrowed;
        return {};
    }

    status_t deserialize_char(char& value) {
        auto parsed = read_u8();
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        value = static_cast<char>(*parsed);
        return {};
    }

    status_t deserialize_str(std::string& value) {
        auto length = read_length();
        if(!length) {
            return std::unexpected(length.error());
        }

        if(offset + *length > bytes.size()) {
            return mark_invalid(error_type::unexpected_eof);
        }

        if(*length == 0) {
            value.clear();
            return {};
        }

        const auto* begin = reinterpret_cast<const char*>(bytes.data() + offset);
        value.assign(begin, begin + *length);
        offset += *length;
        return {};
    }

    status_t deserialize_bytes(std::vector<std::byte>& value) {
        auto length = read_length();
        if(!length) {
            return std::unexpected(length.error());
        }

        if(offset + *length > bytes.size()) {
            return mark_invalid(error_type::unexpected_eof);
        }

        if(*length == 0) {
            value.clear();
            return {};
        }

        value.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                     bytes.begin() + static_cast<std::ptrdiff_t>(offset + *length));
        offset += *length;
        return {};
    }

    result_t<bool> deserialize_none() {
        auto tag = read_u8();
        if(!tag) {
            return std::unexpected(tag.error());
        }

        if(*tag == 0U) {
            return true;
        }
        if(*tag == 1U) {
            return false;
        }
        return std::unexpected(error_type::invalid_type);
    }

    template <typename... Ts>
    status_t deserialize_variant(std::variant<Ts...>& value) {
        auto index = read_integral<std::uint32_t>();
        if(!index) {
            return std::unexpected(index.error());
        }

        constexpr std::size_t variant_size = sizeof...(Ts);
        if(*index >= variant_size) {
            return mark_invalid(error_type::invalid_variant_index);
        }

        std::expected<void, error_type> status{};
        bool matched = false;

        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (([&] {
                 if(*index != I) {
                     return;
                 }

                 matched = true;
                 status = deserialize_variant_alternative<I>(value);
             }()),
             ...);
        }(std::make_index_sequence<variant_size>{});

        if(!matched) {
            return mark_invalid(error_type::invalid_variant_index);
        }
        if(!status) {
            return std::unexpected(status.error());
        }
        return {};
    }

    result_t<DeserializeSeq> deserialize_seq(std::optional<std::size_t> len) {
        auto parsed = read_length();
        if(!parsed) {
            return std::unexpected(parsed.error());
        }

        if(len.has_value() && *len != *parsed) {
            return std::unexpected(error_type::invalid_state);
        }

        return DeserializeSeq(*this, *parsed);
    }

    result_t<DeserializeTuple> deserialize_tuple(std::size_t len) {
        return DeserializeTuple(*this, len);
    }

    result_t<DeserializeMap> deserialize_map(std::optional<std::size_t> /*len*/) {
        return DeserializeUnsupported(*this);
    }

    result_t<DeserializeStruct> deserialize_struct(std::string_view /*name*/, std::size_t /*len*/) {
        return DeserializeUnsupported(*this);
    }

    result_t<std::size_t> read_length_prefix() {
        return read_length();
    }

private:
    template <std::size_t I, typename Variant>
    status_t deserialize_variant_alternative(Variant& value) {
        using alt_t = std::variant_alternative_t<I, Variant>;
        if constexpr(std::same_as<alt_t, std::monostate>) {
            value = std::monostate{};
            return {};
        } else if constexpr(std::default_initializable<alt_t>) {
            alt_t alt{};
            auto status = serde::deserialize(*this, alt);
            if(!status) {
                return std::unexpected(status.error());
            }

            value = std::move(alt);
            return {};
        } else {
            return mark_invalid(error_type::invalid_state);
        }
    }

    template <typename T>
        requires std::integral<T>
    result_t<T> read_integral() {
        if(!is_valid) {
            return std::unexpected(current_error());
        }

        using unsigned_t = std::make_unsigned_t<T>;
        if(offset + sizeof(unsigned_t) > bytes.size()) {
            mark_invalid(error_type::unexpected_eof);
            return std::unexpected(current_error());
        }

        unsigned_t raw = 0;
        for(std::size_t i = 0; i < sizeof(unsigned_t); ++i) {
            const auto byte = std::to_integer<std::uint8_t>(bytes[offset + i]);
            raw |= (static_cast<unsigned_t>(byte) << (i * 8));
        }

        offset += sizeof(unsigned_t);
        if constexpr(std::signed_integral<T>) {
            return std::bit_cast<T>(raw);
        } else {
            return static_cast<T>(raw);
        }
    }

    result_t<std::uint8_t> read_u8() {
        return read_integral<std::uint8_t>();
    }

    result_t<std::size_t> read_length() {
        auto raw = read_integral<std::uint64_t>();
        if(!raw) {
            return std::unexpected(raw.error());
        }

        if(*raw > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
            mark_invalid(error_type::number_out_of_range);
            return std::unexpected(current_error());
        }

        return static_cast<std::size_t>(*raw);
    }

    status_t mark_invalid(error_type error) {
        is_valid = false;
        last_error = error;
        return std::unexpected(error);
    }

    error_type current_error() const {
        return is_valid ? error_type::ok : last_error;
    }

private:
    std::span<const std::byte> bytes{};
    std::size_t offset = 0;
    bool is_valid = true;
    error_type last_error = error_type::ok;
};

template <typename T>
auto to_bytes(const T& value) -> std::expected<std::vector<std::byte>, error_kind> {
    Serializer serializer;
    auto status = serde::serialize(serializer, value);
    if(!status) {
        return std::unexpected(status.error());
    }
    if(!serializer.valid()) {
        return std::unexpected(serializer.error());
    }
    return serializer.take_bytes();
}

template <typename T>
auto from_bytes(std::span<const std::byte> bytes, T& value) -> std::expected<void, error_kind> {
    Deserializer deserializer(bytes);
    if(!deserializer.valid()) {
        return std::unexpected(deserializer.error());
    }

    auto status = serde::deserialize(deserializer, value);
    if(!status) {
        return std::unexpected(status.error());
    }

    auto finished = deserializer.finish();
    if(!finished) {
        return std::unexpected(finished.error());
    }
    return {};
}

template <typename T>
auto from_bytes(std::span<const std::uint8_t> bytes, T& value) -> std::expected<void, error_kind> {
    return from_bytes(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()),
        value);
}

template <typename T>
auto from_bytes(const std::vector<std::byte>& bytes, T& value) -> std::expected<void, error_kind> {
    return from_bytes(std::span<const std::byte>(bytes.data(), bytes.size()), value);
}

template <typename T>
auto from_bytes(const std::vector<std::uint8_t>& bytes, T& value)
    -> std::expected<void, error_kind> {
    return from_bytes(std::span<const std::uint8_t>(bytes.data(), bytes.size()), value);
}

template <typename T>
    requires std::default_initializable<T>
auto from_bytes(std::span<const std::byte> bytes) -> std::expected<T, error_kind> {
    T value{};
    auto status = from_bytes(bytes, value);
    if(!status) {
        return std::unexpected(status.error());
    }
    return value;
}

template <typename T>
    requires std::default_initializable<T>
auto from_bytes(std::span<const std::uint8_t> bytes) -> std::expected<T, error_kind> {
    T value{};
    auto status = from_bytes(bytes, value);
    if(!status) {
        return std::unexpected(status.error());
    }
    return value;
}

template <typename T>
    requires std::default_initializable<T>
auto from_bytes(const std::vector<std::byte>& bytes) -> std::expected<T, error_kind> {
    return from_bytes<T>(std::span<const std::byte>(bytes.data(), bytes.size()));
}

template <typename T>
    requires std::default_initializable<T>
auto from_bytes(const std::vector<std::uint8_t>& bytes) -> std::expected<T, error_kind> {
    return from_bytes<T>(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
}

static_assert(serde::serializer_like<Serializer>);
static_assert(serde::deserializer_like<Deserializer>);

}  // namespace eventide::serde::bincode

namespace eventide::serde {

template <typename T>
    requires (refl::reflectable_class<std::remove_cvref_t<T>> &&
              !std::ranges::input_range<std::remove_cvref_t<T>>)
struct deserialize_traits<bincode::Deserializer, T> {
    using deserializer_t = bincode::Deserializer;
    using error_type = typename deserializer_t::error_type;

    static auto deserialize(deserializer_t& deserializer, T& value)
        -> std::expected<void, error_type> {
        std::expected<void, error_type> field_status{};

        refl::for_each(value, [&](auto field) {
            auto status = serde::deserialize(deserializer, field.value());
            if(!status) {
                field_status = std::unexpected(status.error());
                return false;
            }
            return true;
        });

        if(!field_status) {
            return std::unexpected(field_status.error());
        }
        return {};
    }
};

template <typename T>
    requires (std::ranges::input_range<std::remove_cvref_t<T>> &&
              format_kind<std::remove_cvref_t<T>> == range_format::map)
struct deserialize_traits<bincode::Deserializer, T> {
    using deserializer_t = bincode::Deserializer;
    using error_type = typename deserializer_t::error_type;
    using map_t = std::remove_cvref_t<T>;
    using key_t = typename map_t::key_type;
    using mapped_t = typename map_t::mapped_type;

    static auto deserialize(deserializer_t& deserializer, T& value)
        -> std::expected<void, error_type> {
        static_assert(std::default_initializable<key_t>,
                      "bincode map deserialization requires default-constructible key_type");
        static_assert(std::default_initializable<mapped_t>,
                      "bincode map deserialization requires default-constructible mapped_type");
        static_assert(eventide::detail::map_insertable<map_t, key_t, mapped_t>,
                      "bincode map deserialization requires insertable map container");

        auto length = deserializer.read_length_prefix();
        if(!length) {
            return std::unexpected(length.error());
        }

        if constexpr(requires { value.clear(); }) {
            value.clear();
        }

        for(std::size_t i = 0; i < *length; ++i) {
            key_t key{};
            auto key_status = serde::deserialize(deserializer, key);
            if(!key_status) {
                return std::unexpected(key_status.error());
            }

            mapped_t mapped{};
            auto mapped_status = serde::deserialize(deserializer, mapped);
            if(!mapped_status) {
                return std::unexpected(mapped_status.error());
            }

            eventide::detail::insert_map_entry(value, std::move(key), std::move(mapped));
        }

        return {};
    }
};

}  // namespace eventide::serde
