#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "eventide/common/expected_try.h"
#include "eventide/common/ranges.h"
#include "eventide/serde/flatbuffers/schema.h"
#include "eventide/serde/flatbuffers/serializer.h"
#include "eventide/serde/serde/config.h"
#include "eventide/serde/serde/utils/common.h"
#include "eventide/serde/serde/utils/positional.h"

#if __has_include(<flatbuffers/flatbuffers.h>)
#include <flatbuffers/flatbuffers.h>
#else
#error                                                                                             \
    "flatbuffers/flatbuffers.h not found. Enable ETD_SERDE_ENABLE_FLATBUFFERS or add flatbuffers include paths."
#endif

namespace eventide::serde::flatbuffers {

namespace deserialize_detail {

constexpr ::flatbuffers::voffset_t first_field = 4;
constexpr ::flatbuffers::voffset_t field_step = 2;

template <typename T>
using result_t = std::expected<T, object_error_code>;

using status_t = result_t<void>;

using serde::detail::clean_t;
using serde::detail::remove_annotation_t;
using serde::detail::remove_optional_t;

template <typename T>
constexpr bool root_unboxed_v =
    (refl::reflectable_class<T> && !can_inline_struct_v<T>) || is_pair_v<T> || is_tuple_v<T> ||
    is_specialization_of<std::variant, T>;

inline auto field_voffset(std::size_t index) -> ::flatbuffers::voffset_t {
    return static_cast<::flatbuffers::voffset_t>(static_cast<std::size_t>(first_field) +
                                                 index * static_cast<std::size_t>(field_step));
}

inline auto variant_field_voffset(std::size_t index) -> ::flatbuffers::voffset_t {
    return field_voffset(index + 1);
}

inline auto has_field(const ::flatbuffers::Table* table, ::flatbuffers::voffset_t field) -> bool {
    return table != nullptr && table->GetOptionalFieldOffset(field) != 0;
}

inline auto table_has_any_field(const ::flatbuffers::Table* table) -> bool {
    if(table == nullptr) {
        return false;
    }

    const auto* vtable = table->GetVTable();
    if(vtable == nullptr) {
        return false;
    }

    const auto vtable_size = ::flatbuffers::ReadScalar<::flatbuffers::voffset_t>(vtable);
    return vtable_size > static_cast<::flatbuffers::voffset_t>(4);
}

class FlatbufferFieldDeserializer {
public:
    using error_type = flatbuffers::error;

    template <typename T>
    using result_t = std::expected<T, error_type>;
    using status_t = result_t<void>;

    class DeserializeUnsupported {
    public:
        result_t<bool> has_next() { return std::unexpected(error_type::invalid_state); }

        result_t<std::optional<std::string_view>> next_key() {
            return std::unexpected(error_type::invalid_state);
        }

        template <typename T>
        status_t deserialize_element(T&) {
            return std::unexpected(error_type::invalid_state);
        }

        template <typename T>
        status_t deserialize_value(T&) {
            return std::unexpected(error_type::invalid_state);
        }

        status_t skip_element() { return std::unexpected(error_type::invalid_state); }
        status_t skip_value() { return std::unexpected(error_type::invalid_state); }
        status_t end() { return std::unexpected(error_type::invalid_state); }
    };

    using DeserializeSeq = DeserializeUnsupported;
    using DeserializeTuple = DeserializeUnsupported;
    using DeserializeMap = DeserializeUnsupported;
    using DeserializeStruct = DeserializeUnsupported;

    FlatbufferFieldDeserializer(const ::flatbuffers::Table* table, ::flatbuffers::voffset_t voffset) :
        table_(table), voffset_(voffset) {}

    // Primitive methods

    status_t deserialize_bool(bool& value) {
        value = table_->GetField<bool>(voffset_, false);
        return {};
    }

    template <serde::int_like T>
    status_t deserialize_int(T& value) {
        value = table_->GetField<T>(voffset_, T{});
        return {};
    }

    template <serde::uint_like T>
    status_t deserialize_uint(T& value) {
        value = table_->GetField<T>(voffset_, T{});
        return {};
    }

    template <serde::floating_like T>
    status_t deserialize_float(T& value) {
        if constexpr(std::same_as<T, float> || std::same_as<T, double>) {
            value = table_->GetField<T>(voffset_, T{});
        } else {
            value = static_cast<T>(table_->GetField<double>(voffset_, 0.0));
        }
        return {};
    }

    status_t deserialize_char(char& value) {
        value = static_cast<char>(table_->GetField<std::int8_t>(voffset_, 0));
        return {};
    }

    status_t deserialize_str(std::string& value) {
        const auto* text = table_->GetPointer<const ::flatbuffers::String*>(voffset_);
        if(text == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        value.assign(text->data(), text->size());
        return {};
    }

    status_t deserialize_bytes(std::vector<std::byte>& value) {
        const auto* bytes = table_->GetPointer<const ::flatbuffers::Vector<std::uint8_t>*>(voffset_);
        if(bytes == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        value.resize(bytes->size());
        for(std::size_t i = 0; i < bytes->size(); ++i) {
            value[i] = std::byte{bytes->Get(static_cast<::flatbuffers::uoffset_t>(i))};
        }
        return {};
    }

    // None / optional support

    result_t<bool> deserialize_none() {
        return !has_field(table_, voffset_);
    }

    // Variant support — defined inline, uses serde::deserialize (template lazy instantiation)

    template <typename... Ts>
    status_t deserialize_variant(std::variant<Ts...>& value) {
        const auto* nested = table_->GetPointer<const ::flatbuffers::Table*>(voffset_);
        return decode_variant_table(nested, value);
    }

    // Unsupported composite protocol methods (handled by deserialize_traits)

    result_t<DeserializeSeq> deserialize_seq(std::optional<std::size_t>) {
        return std::unexpected(error_type::invalid_state);
    }

    result_t<DeserializeTuple> deserialize_tuple(std::size_t) {
        return std::unexpected(error_type::invalid_state);
    }

    result_t<DeserializeMap> deserialize_map(std::optional<std::size_t>) {
        return std::unexpected(error_type::invalid_state);
    }

    result_t<DeserializeStruct> deserialize_struct(std::string_view, std::size_t) {
        return std::unexpected(error_type::invalid_state);
    }

    // Accessors for deserialize_traits

    const ::flatbuffers::Table* table() const { return table_; }
    ::flatbuffers::voffset_t voffset() const { return voffset_; }

    // Static variant decoder (shared with root-level dispatch)

    template <typename... Ts>
    static auto decode_variant_table(const ::flatbuffers::Table* table,
                                     std::variant<Ts...>& out) -> status_t {
        using U = std::variant<Ts...>;
        if(table == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        if(!has_field(table, first_field)) {
            return std::unexpected(error_type::invalid_state);
        }

        const auto index =
            static_cast<std::size_t>(table->GetField<std::uint32_t>(first_field, 0U));
        if(index >= std::variant_size_v<U>) {
            return std::unexpected(error_type::invalid_state);
        }

        status_t status{};
        bool matched = false;
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (([&] {
                 if(index != I) {
                     return;
                 }
                 matched = true;
                 using alt_t = std::variant_alternative_t<I, U>;
                 if constexpr(!std::default_initializable<alt_t>) {
                     status = std::unexpected(error_type::invalid_state);
                 } else {
                     alt_t alt{};
                     FlatbufferFieldDeserializer alt_d(table, variant_field_voffset(I));
                     auto decoded = serde::deserialize(alt_d, alt);
                     if(!decoded) {
                         status = std::unexpected(decoded.error());
                         return;
                     }
                     out = std::move(alt);
                 }
             }()),
             ...);
        }(std::make_index_sequence<std::variant_size_v<U>>{});

        if(!matched) {
            return std::unexpected(error_type::invalid_state);
        }
        return status;
    }

private:
    const ::flatbuffers::Table* table_ = nullptr;
    ::flatbuffers::voffset_t voffset_ = 0;
};

static_assert(serde::deserializer_like<FlatbufferFieldDeserializer>);

// --- Free functions for composite type deserialization ---

template <typename T>
auto decode_struct_from_table(const ::flatbuffers::Table* table, T& out)
    -> FlatbufferFieldDeserializer::status_t {
    using error_type = FlatbufferFieldDeserializer::error_type;
    if(table == nullptr) {
        return std::unexpected(error_type::invalid_state);
    }
    return serde::detail::deserialize_positional_struct<
        config::default_config, error_type, serde::detail::indexed_policy>(
        out,
        [&](std::size_t index, auto& v) -> std::expected<void, error_type> {
            auto voff = field_voffset(index);
            if(!has_field(table, voff)) {
                return {};
            }
            FlatbufferFieldDeserializer field_d(table, voff);
            return serde::deserialize(field_d, v);
        },
        [&](auto tag, auto& v) -> std::expected<void, error_type> {
            static_assert(dependent_false<decltype(tag)>,
                          "behavior::with is not supported for flatbuffers deserialization");
            return {};
        });
}

template <typename T>
auto decode_tuple_from_table(const ::flatbuffers::Table* table, T& out)
    -> FlatbufferFieldDeserializer::status_t {
    using error_type = FlatbufferFieldDeserializer::error_type;
    if(table == nullptr) {
        return std::unexpected(error_type::invalid_state);
    }
    std::expected<void, error_type> status{};
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((status.has_value() ? [&] {
             auto voff = field_voffset(I);
             if(!has_field(table, voff)) {
                 return;  // keep default value for absent fields
             }
             FlatbufferFieldDeserializer field_d(table, voff);
             status = serde::deserialize(field_d, std::get<I>(out));
         }()
                             : void()),
         ...);
    }(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<T>>>{});
    return status;
}

/// Decode a boxed/unboxed element from a per-element table (used by sequence and map).
/// Replicates the root-value dispatch: unboxed types (struct/tuple/variant) are decoded
/// directly from the table; boxed types read from first_field via FBFD.
template <typename T>
auto decode_element_from_table(const ::flatbuffers::Table* table, T& out)
    -> FlatbufferFieldDeserializer::status_t {
    using error_type = FlatbufferFieldDeserializer::error_type;
    using U = std::remove_cvref_t<T>;
    using clean_u_t = clean_t<U>;

    if constexpr(serde::annotated_type<U>) {
        return decode_element_from_table(table, serde::annotated_value(out));
    } else if constexpr(root_unboxed_v<clean_u_t>) {
        if constexpr(refl::reflectable_class<clean_u_t> && !can_inline_struct_v<clean_u_t>) {
            return decode_struct_from_table(table, out);
        } else if constexpr(is_pair_v<clean_u_t> || is_tuple_v<clean_u_t>) {
            return decode_tuple_from_table(table, out);
        } else if constexpr(is_specialization_of<std::variant, clean_u_t>) {
            return FlatbufferFieldDeserializer::decode_variant_table(table, out);
        } else {
            return std::unexpected(error_type::invalid_state);
        }
    } else {
        if(table == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        FlatbufferFieldDeserializer field_d(table, first_field);
        return serde::deserialize(field_d, out);
    }
}

template <typename T>
auto decode_sequence_from_field(const ::flatbuffers::Table* table,
                                ::flatbuffers::voffset_t field,
                                T& out) -> FlatbufferFieldDeserializer::status_t {
    using error_type = FlatbufferFieldDeserializer::error_type;
    using U = std::remove_cvref_t<T>;
    using element_t = std::ranges::range_value_t<U>;
    using element_clean_t = clean_t<element_t>;

    if(!has_field(table, field)) {
        return {};
    }

    if constexpr(requires { out.clear(); }) {
        out.clear();
    }

    constexpr bool index_assignable_fixed_size =
        !eventide::detail::sequence_insertable<U, element_t> &&
        requires(U& container, std::size_t index, element_t value) {
            std::tuple_size<U>::value;
            container[index] = std::move(value);
        };

    std::size_t written_count = 0;
    auto store_element = [&](auto&& element) -> FlatbufferFieldDeserializer::status_t {
        if constexpr(index_assignable_fixed_size) {
            constexpr auto expected_count = std::tuple_size_v<U>;
            if(written_count >= expected_count) {
                return std::unexpected(error_type::invalid_state);
            }
            out[written_count] =
                static_cast<element_t>(std::forward<decltype(element)>(element));
            ++written_count;
            return {};
        } else {
            auto ok = eventide::detail::append_sequence_element(
                out,
                static_cast<element_t>(std::forward<decltype(element)>(element)));
            if(!ok) {
                return std::unexpected(error_type(object_error_code::unsupported_type));
            }
            return {};
        }
    };

    auto finalize_sequence = [&]() -> FlatbufferFieldDeserializer::status_t {
        if constexpr(index_assignable_fixed_size) {
            constexpr auto expected_count = std::tuple_size_v<U>;
            if(written_count != expected_count) {
                return std::unexpected(error_type::invalid_state);
            }
        }
        return {};
    };

    if constexpr(std::same_as<element_clean_t, std::byte>) {
        const auto* vector =
            table->GetPointer<const ::flatbuffers::Vector<std::uint8_t>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            ETD_EXPECTED_TRY(store_element(
                std::byte{vector->Get(static_cast<::flatbuffers::uoffset_t>(i))}));
        }
        return finalize_sequence();
    } else if constexpr(serde::bool_like<element_clean_t> || serde::int_like<element_clean_t> ||
                         serde::uint_like<element_clean_t>) {
        const auto* vector =
            table->GetPointer<const ::flatbuffers::Vector<element_clean_t>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            ETD_EXPECTED_TRY(
                store_element(vector->Get(static_cast<::flatbuffers::uoffset_t>(i))));
        }
        return finalize_sequence();
    } else if constexpr(serde::floating_like<element_clean_t>) {
        if constexpr(std::same_as<element_clean_t, float> ||
                     std::same_as<element_clean_t, double>) {
            const auto* vector =
                table->GetPointer<const ::flatbuffers::Vector<element_clean_t>*>(field);
            if(vector == nullptr) {
                return std::unexpected(error_type::invalid_state);
            }
            for(std::size_t i = 0; i < vector->size(); ++i) {
                ETD_EXPECTED_TRY(
                    store_element(vector->Get(static_cast<::flatbuffers::uoffset_t>(i))));
            }
            return finalize_sequence();
        } else {
            const auto* vector = table->GetPointer<const ::flatbuffers::Vector<double>*>(field);
            if(vector == nullptr) {
                return std::unexpected(error_type::invalid_state);
            }
            for(std::size_t i = 0; i < vector->size(); ++i) {
                ETD_EXPECTED_TRY(
                    store_element(vector->Get(static_cast<::flatbuffers::uoffset_t>(i))));
            }
            return finalize_sequence();
        }
    } else if constexpr(serde::char_like<element_clean_t>) {
        const auto* vector =
            table->GetPointer<const ::flatbuffers::Vector<std::int8_t>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            ETD_EXPECTED_TRY(store_element(
                static_cast<char>(vector->Get(static_cast<::flatbuffers::uoffset_t>(i)))));
        }
        return finalize_sequence();
    } else if constexpr(std::is_enum_v<element_clean_t>) {
        using storage_t = std::underlying_type_t<element_clean_t>;
        const auto* vector = table->GetPointer<const ::flatbuffers::Vector<storage_t>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            ETD_EXPECTED_TRY(store_element(static_cast<element_clean_t>(
                vector->Get(static_cast<::flatbuffers::uoffset_t>(i)))));
        }
        return finalize_sequence();
    } else if constexpr(serde::str_like<element_clean_t>) {
        const auto* vector = table->GetPointer<
            const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            const auto* text = vector->GetAsString(static_cast<::flatbuffers::uoffset_t>(i));
            if(text == nullptr) {
                return std::unexpected(error_type::invalid_state);
            }
            if constexpr(std::same_as<element_t, std::string> ||
                         std::derived_from<element_t, std::string>) {
                ETD_EXPECTED_TRY(store_element(std::string(text->data(), text->size())));
            } else if constexpr(std::same_as<element_t, std::string_view>) {
                ETD_EXPECTED_TRY(store_element(std::string_view(text->data(), text->size())));
            } else {
                return std::unexpected(error_type(object_error_code::unsupported_type));
            }
        }
        return finalize_sequence();
    } else if constexpr(is_pair_v<element_clean_t> || is_tuple_v<element_clean_t>) {
        const auto* vector = table->GetPointer<
            const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::Table>>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            using dec_t = std::remove_cvref_t<element_t>;
            if constexpr(!std::default_initializable<dec_t>) {
                return std::unexpected(error_type(object_error_code::unsupported_type));
            } else {
                dec_t element{};
                const auto* nested = vector->GetAs<::flatbuffers::Table>(
                    static_cast<::flatbuffers::uoffset_t>(i));
                ETD_EXPECTED_TRY(decode_tuple_from_table(nested, element));
                ETD_EXPECTED_TRY(store_element(std::move(element)));
            }
        }
        return finalize_sequence();
    } else if constexpr(can_inline_struct_v<element_clean_t>) {
        const auto* vector =
            table->GetPointer<const ::flatbuffers::Vector<const element_clean_t*>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            const auto* element = vector->Get(static_cast<::flatbuffers::uoffset_t>(i));
            if(element == nullptr) {
                return std::unexpected(error_type::invalid_state);
            }
            ETD_EXPECTED_TRY(store_element(*element));
        }
        return finalize_sequence();
    } else if constexpr(refl::reflectable_class<element_clean_t>) {
        const auto* vector = table->GetPointer<
            const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::Table>>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            using dec_t = std::remove_cvref_t<element_t>;
            if constexpr(!std::default_initializable<dec_t>) {
                return std::unexpected(error_type(object_error_code::unsupported_type));
            } else {
                dec_t element{};
                const auto* nested = vector->GetAs<::flatbuffers::Table>(
                    static_cast<::flatbuffers::uoffset_t>(i));
                ETD_EXPECTED_TRY(decode_struct_from_table(nested, element));
                ETD_EXPECTED_TRY(store_element(std::move(element)));
            }
        }
        return finalize_sequence();
    } else {
        const auto* vector = table->GetPointer<
            const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::Table>>*>(field);
        if(vector == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        for(std::size_t i = 0; i < vector->size(); ++i) {
            using dec_t = std::remove_cvref_t<element_t>;
            if constexpr(!std::default_initializable<dec_t>) {
                return std::unexpected(error_type(object_error_code::unsupported_type));
            } else {
                dec_t element{};
                const auto* nested = vector->GetAs<::flatbuffers::Table>(
                    static_cast<::flatbuffers::uoffset_t>(i));
                ETD_EXPECTED_TRY(decode_element_from_table(nested, element));
                ETD_EXPECTED_TRY(store_element(std::move(element)));
            }
        }
        return finalize_sequence();
    }
}

template <typename T>
auto decode_map_from_field(const ::flatbuffers::Table* table,
                           ::flatbuffers::voffset_t field,
                           T& out) -> FlatbufferFieldDeserializer::status_t {
    using error_type = FlatbufferFieldDeserializer::error_type;
    using U = std::remove_cvref_t<T>;
    using key_t = typename U::key_type;
    using mapped_t = typename U::mapped_type;

    if(!has_field(table, field)) {
        return {};
    }

    if constexpr(requires { out.clear(); }) {
        out.clear();
    }
    static_assert(eventide::detail::map_insertable<U, key_t, mapped_t>,
                  "from_flatbuffer map requires insertable container");

    const auto* entries = table->GetPointer<
        const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::Table>>*>(field);
    if(entries == nullptr) {
        return std::unexpected(error_type::invalid_state);
    }

    for(std::size_t i = 0; i < entries->size(); ++i) {
        auto* entry =
            entries->GetAs<::flatbuffers::Table>(static_cast<::flatbuffers::uoffset_t>(i));
        if(entry == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }

        key_t key{};
        mapped_t mapped{};
        FlatbufferFieldDeserializer key_d(entry, first_field);
        ETD_EXPECTED_TRY(serde::deserialize(key_d, key));
        FlatbufferFieldDeserializer val_d(entry, field_voffset(1));
        ETD_EXPECTED_TRY(serde::deserialize(val_d, mapped));

        auto ok = eventide::detail::insert_map_entry(out, std::move(key), std::move(mapped));
        if(!ok) {
            return std::unexpected(error_type(object_error_code::unsupported_type));
        }
    }

    return {};
}


}  // namespace deserialize_detail

template <typename Config = config::default_config>
class Deserializer {
public:
    using config_type = Config;
    using error_type = flatbuffers::error;

    template <typename T>
    using result_t = std::expected<T, error_type>;

    explicit Deserializer(std::span<const std::uint8_t> bytes) {
        initialize(bytes);
    }

    explicit Deserializer(std::span<const std::byte> bytes) {
        if(bytes.empty()) {
            initialize(std::span<const std::uint8_t>{});
            return;
        }
        const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
        initialize(std::span<const std::uint8_t>(data, bytes.size()));
    }

    explicit Deserializer(const std::vector<std::uint8_t>& bytes) :
        Deserializer(std::span<const std::uint8_t>(bytes.data(), bytes.size())) {}

    auto valid() const -> bool {
        return is_valid;
    }

    auto error() const -> error_type {
        if(is_valid) {
            return object_error_code::ok;
        }
        return last_error;
    }

    template <typename T>
    auto deserialize(T& value) const -> result_t<void> {
        if(!is_valid || root == nullptr) {
            return std::unexpected(last_error);
        }
        return deserialize_root_value(value);
    }

private:
    template <typename T>
    auto deserialize_root_value(T& out) const -> result_t<void> {
        using U = std::remove_cvref_t<T>;
        using clean_u_t = deserialize_detail::clean_t<U>;

        if constexpr(serde::annotated_type<U>) {
            return deserialize_root_value(serde::annotated_value(out));
        } else if constexpr(is_specialization_of<std::optional, U>) {
            using value_t = typename U::value_type;
            using clean_value_t = deserialize_detail::clean_t<value_t>;

            if constexpr(deserialize_detail::root_unboxed_v<clean_value_t>) {
                if(!deserialize_detail::table_has_any_field(root)) {
                    out.reset();
                    return {};
                }

                if(!out.has_value()) {
                    if constexpr(std::default_initializable<value_t>) {
                        out.emplace();
                    } else {
                        return std::unexpected(error_type(object_error_code::unsupported_type));
                    }
                }

                auto status = deserialize_root_unboxed(out.value());
                if(!status) {
                    out.reset();
                    return std::unexpected(status.error());
                }
                return {};
            } else {
                // Boxed optional: FBFD at first_field, serde handles optional
                deserialize_detail::FlatbufferFieldDeserializer field_d(
                    root, deserialize_detail::first_field);
                return serde::deserialize(field_d, out);
            }
        } else if constexpr(deserialize_detail::root_unboxed_v<clean_u_t>) {
            return deserialize_root_unboxed(out);
        } else {
            deserialize_detail::FlatbufferFieldDeserializer field_d(
                root, deserialize_detail::first_field);
            return serde::deserialize(field_d, out);
        }
    }

    template <typename T>
    auto deserialize_root_unboxed(T& out) const -> result_t<void> {
        using U = std::remove_cvref_t<T>;
        using clean_u_t = deserialize_detail::clean_t<U>;

        if constexpr(refl::reflectable_class<clean_u_t> && !can_inline_struct_v<clean_u_t>) {
            return deserialize_detail::decode_struct_from_table(root, out);
        } else if constexpr(is_pair_v<clean_u_t> || is_tuple_v<clean_u_t>) {
            return deserialize_detail::decode_tuple_from_table(root, out);
        } else if constexpr(is_specialization_of<std::variant, clean_u_t>) {
            return deserialize_detail::FlatbufferFieldDeserializer::decode_variant_table(root, out);
        } else {
            return std::unexpected(error_type::invalid_state);
        }
    }

    auto initialize(std::span<const std::uint8_t> bytes) -> void {
        if(bytes.empty()) {
            set_invalid(object_error_code::invalid_state);
            return;
        }
        if(!::flatbuffers::BufferHasIdentifier(
               bytes.data(),
               ::eventide::serde::flatbuffers::detail::buffer_identifier)) {
            set_invalid(object_error_code::invalid_state);
            return;
        }

        auto* table = ::flatbuffers::GetRoot<::flatbuffers::Table>(bytes.data());
        if(table == nullptr) {
            set_invalid(object_error_code::invalid_state);
            return;
        }

        ::flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        if(!table->VerifyTableStart(verifier) || !verifier.EndTable()) {
            set_invalid(object_error_code::invalid_state);
            return;
        }

        root = table;
    }

    auto set_invalid(error_type err) -> void {
        is_valid = false;
        last_error = err;
        root = nullptr;
    }

private:
    bool is_valid = true;
    error_type last_error = object_error_code::ok;
    const ::flatbuffers::Table* root = nullptr;
};

template <typename Config = config::default_config, typename T>
auto from_flatbuffer(std::span<const std::uint8_t> bytes, T& value)
    -> std::expected<void, flatbuffers::error> {
    Deserializer<Config> deserializer(bytes);
    if(!deserializer.valid()) {
        return std::unexpected(deserializer.error());
    }

    ETD_EXPECTED_TRY(deserializer.deserialize(value));
    return {};
}

template <typename Config = config::default_config, typename T>
auto from_flatbuffer(std::span<const std::byte> bytes, T& value)
    -> std::expected<void, flatbuffers::error> {
    Deserializer<Config> deserializer(bytes);
    if(!deserializer.valid()) {
        return std::unexpected(deserializer.error());
    }

    ETD_EXPECTED_TRY(deserializer.deserialize(value));
    return {};
}

template <typename Config = config::default_config, typename T>
auto from_flatbuffer(const std::vector<std::uint8_t>& bytes, T& value)
    -> std::expected<void, flatbuffers::error> {
    return from_flatbuffer<Config>(std::span<const std::uint8_t>(bytes.data(), bytes.size()),
                                   value);
}

template <typename T, typename Config = config::default_config>
    requires std::default_initializable<T>
auto from_flatbuffer(std::span<const std::uint8_t> bytes) -> std::expected<T, flatbuffers::error> {
    T value{};
    ETD_EXPECTED_TRY(from_flatbuffer<Config>(bytes, value));
    return value;
}

template <typename T, typename Config = config::default_config>
    requires std::default_initializable<T>
auto from_flatbuffer(std::span<const std::byte> bytes) -> std::expected<T, flatbuffers::error> {
    T value{};
    ETD_EXPECTED_TRY(from_flatbuffer<Config>(bytes, value));
    return value;
}

template <typename T, typename Config = config::default_config>
    requires std::default_initializable<T>
auto from_flatbuffer(const std::vector<std::uint8_t>& bytes)
    -> std::expected<T, flatbuffers::error> {
    return from_flatbuffer<T, Config>(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
}

}  // namespace eventide::serde::flatbuffers

namespace eventide::serde {

// --- deserialize_traits specializations for FlatbufferFieldDeserializer ---

using flatbuffers_fbfd = flatbuffers::deserialize_detail::FlatbufferFieldDeserializer;

// Enums: read at exact underlying type width (flatbuffers stores enums at their precise type)
template <typename T>
    requires std::is_enum_v<std::remove_cvref_t<T>>
struct deserialize_traits<flatbuffers_fbfd, T> {
    using deserializer_t = flatbuffers_fbfd;
    using error_type = typename deserializer_t::error_type;

    static auto deserialize(deserializer_t& d, T& value) -> std::expected<void, error_type> {
        using U = std::remove_cvref_t<T>;
        using underlying_t = std::underlying_type_t<U>;
        underlying_t raw{};
        if constexpr(int_like<underlying_t>) {
            ETD_EXPECTED_TRY(d.deserialize_int(raw));
        } else if constexpr(uint_like<underlying_t>) {
            ETD_EXPECTED_TRY(d.deserialize_uint(raw));
        } else {
            static_assert(dependent_false<T>, "unsupported enum underlying type");
        }
        value = static_cast<T>(raw);
        return {};
    }
};

// Reflectable struct (non-inline, non-range): sub-table → positional deserialization
template <typename T>
    requires(refl::reflectable_class<std::remove_cvref_t<T>> &&
             !flatbuffers::can_inline_struct_v<std::remove_cvref_t<T>> &&
             !std::ranges::input_range<std::remove_cvref_t<T>>)
struct deserialize_traits<flatbuffers_fbfd, T> {
    using deserializer_t = flatbuffers_fbfd;
    using error_type = typename deserializer_t::error_type;

    static auto deserialize(deserializer_t& d, T& value) -> std::expected<void, error_type> {
        const auto* sub_table =
            d.table()->GetPointer<const ::flatbuffers::Table*>(d.voffset());
        if(sub_table == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        return flatbuffers::deserialize_detail::decode_struct_from_table(sub_table, value);
    }
};

// Inline struct: GetStruct memcpy
template <typename T>
    requires(flatbuffers::can_inline_struct_v<std::remove_cvref_t<T>> &&
             !std::ranges::input_range<std::remove_cvref_t<T>>)
struct deserialize_traits<flatbuffers_fbfd, T> {
    using deserializer_t = flatbuffers_fbfd;
    using error_type = typename deserializer_t::error_type;

    static auto deserialize(deserializer_t& d, T& value) -> std::expected<void, error_type> {
        using U = std::remove_cvref_t<T>;
        const auto* data = d.table()->GetStruct<const U*>(d.voffset());
        if(data == nullptr) {
            return std::unexpected(error_type::invalid_state);
        }
        value = *data;
        return {};
    }
};

// Sequences (non-map ranges)
template <typename T>
    requires(std::ranges::input_range<std::remove_cvref_t<T>> &&
             eventide::format_kind<std::remove_cvref_t<T>> != eventide::range_format::map)
struct deserialize_traits<flatbuffers_fbfd, T> {
    using deserializer_t = flatbuffers_fbfd;
    using error_type = typename deserializer_t::error_type;

    static auto deserialize(deserializer_t& d, T& value) -> std::expected<void, error_type> {
        return flatbuffers::deserialize_detail::decode_sequence_from_field(
            d.table(), d.voffset(), value);
    }
};

// Maps
template <typename T>
    requires(std::ranges::input_range<std::remove_cvref_t<T>> &&
             eventide::format_kind<std::remove_cvref_t<T>> == eventide::range_format::map)
struct deserialize_traits<flatbuffers_fbfd, T> {
    using deserializer_t = flatbuffers_fbfd;
    using error_type = typename deserializer_t::error_type;

    static auto deserialize(deserializer_t& d, T& value) -> std::expected<void, error_type> {
        return flatbuffers::deserialize_detail::decode_map_from_field(
            d.table(), d.voffset(), value);
    }
};

// Tuples/pairs (not range, not reflectable)
template <typename T>
    requires(tuple_like<std::remove_cvref_t<T>> &&
             !std::ranges::input_range<std::remove_cvref_t<T>> &&
             !refl::reflectable_class<std::remove_cvref_t<T>>)
struct deserialize_traits<flatbuffers_fbfd, T> {
    using deserializer_t = flatbuffers_fbfd;
    using error_type = typename deserializer_t::error_type;

    static auto deserialize(deserializer_t& d, T& value) -> std::expected<void, error_type> {
        const auto* sub_table =
            d.table()->GetPointer<const ::flatbuffers::Table*>(d.voffset());
        return flatbuffers::deserialize_detail::decode_tuple_from_table(sub_table, value);
    }
};

}  // namespace eventide::serde
