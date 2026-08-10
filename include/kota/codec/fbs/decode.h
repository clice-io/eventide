#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "kota/codec/fbs/proxy.h"
#include "kota/codec/fbs/type.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/decode.h"
#include "kota/codec/visit/encode.h"

namespace kota::codec::fbs {

namespace decode_detail {

using fbs::Table;
using fbs::String;
using fbs::Vector;
using fbs::voffset_t;
using fbs::uoffset_t;
using fbs::verifier_t;

struct field_reader;

template <typename T>
struct scalar_reader : detail::visitor_base {
    T value;

    bool visit_bool(bool& out) {
        out = static_cast<bool>(value);
        return true;
    }

    template <typename U>
    bool visit_int(U& out) {
        out = static_cast<U>(value);
        return true;
    }

    template <typename U>
    bool visit_uint(U& out) {
        out = static_cast<U>(value);
        return true;
    }

    template <typename U>
    bool visit_float(U& out) {
        out = static_cast<U>(value);
        return true;
    }

    template <typename U>
    bool visit_char(U& out) {
        out = static_cast<U>(value);
        return true;
    }

    template <typename U, typename Body>
    bool visit_struct(U& out, Body&&) {
        if constexpr(std::is_same_v<std::remove_const_t<U>, T>) {
            out = value;
        }
        return true;
    }
};

struct string_reader : detail::visitor_base {
    std::string_view value;

    template <typename T>
    bool visit_str(T& out) {
        if constexpr(std::same_as<T, std::string>) {
            out.assign(value.data(), value.size());
        } else if constexpr(std::same_as<T, std::string_view>) {
            out = value;
        } else if constexpr(std::constructible_from<T, const char*, std::size_t>) {
            out = T(value.data(), value.size());
        } else {
            out = T(value);
        }
        return true;
    }
};

struct table_field_reader : detail::visitor_base {
    const Table* tbl;
    std::size_t idx = 0;

    template <typename Idx, typename F>
    bool visit_field(Idx, std::string_view, F&& reader);

    template <typename F>
    bool visit_element(F&& reader);
};

template <typename E>
struct vec_reader {
    using element_t = std::remove_cvref_t<E>;

    // An element travels as its resolved representation (behavior attrs, then
    // chained reprs); the reader choice mirrors the encode side through the
    // shared element_layout classification.
    using repr_t = proxy_detail::apply_repr_t<element_t>;

    constexpr static auto layout = proxy_detail::element_layout_of<element_t>();

    using vec_ptr_t = proxy_detail::element_vector_ptr_t<element_t>;

    vec_ptr_t vec;
    uoffset_t idx = 0;

    bool has_element() {
        return vec != nullptr && idx < vec->size();
    }

    template <typename F>
    bool visit_element(F&& reader);
};

struct map_reader {
    const Vector<table_offset_t>* vec;
    uoffset_t idx = 0;

    bool has_entry() {
        return vec != nullptr && idx < vec->size();
    }

    template <typename KF, typename VF>
    bool visit_entry(KF&& key_fn, VF&& val_fn);
};

// (table*, slot) reader: slot > 0 reads from that field slot,
// slot == 0 means the table itself IS the value (e.g. vector element).
struct field_reader : detail::visitor_base {
    const Table* tbl;
    voffset_t slot;

    bool peek_null() {
        if(tbl == nullptr)
            return true;
        return tbl->GetOptionalFieldOffset(slot) == 0;
    }

    bool visit_null() {
        return true;
    }

    bool visit_bool(bool& out) {
        if(tbl == nullptr)
            return true;
        out = static_cast<bool>(tbl->GetField<std::uint8_t>(slot, 0));
        return true;
    }

    template <typename T>
    bool visit_int(T& out) {
        if(tbl == nullptr)
            return true;
        out = static_cast<T>(tbl->GetField<T>(slot, T{}));
        return true;
    }

    template <typename T>
    bool visit_uint(T& out) {
        if(tbl == nullptr)
            return true;
        out = static_cast<T>(tbl->GetField<T>(slot, T{}));
        return true;
    }

    template <typename T>
    bool visit_float(T& out) {
        if(tbl == nullptr)
            return true;
        if constexpr(std::same_as<T, float> || std::same_as<T, double>) {
            out = tbl->GetField<T>(slot, T{});
        } else {
            out = static_cast<T>(tbl->GetField<double>(slot, 0.0));
        }
        return true;
    }

    template <typename T>
    bool visit_str(T& out) {
        if(tbl == nullptr)
            return true;
        const auto* text = tbl->GetPointer<const String*>(slot);
        if(text == nullptr) {
            if constexpr(std::same_as<T, std::string>) {
                out.clear();
            }
            return true;
        }
        if constexpr(std::same_as<T, std::string>) {
            out.assign(text->data(), text->size());
        } else if constexpr(std::same_as<T, std::string_view>) {
            out = std::string_view(text->data(), text->size());
        } else if constexpr(std::constructible_from<T, const char*, std::size_t>) {
            out = T(text->data(), text->size());
        } else {
            std::string_view sv(text->data(), text->size());
            out = T(sv);
        }
        return true;
    }

    template <typename T>
    bool visit_char(T& out) {
        if(tbl == nullptr)
            return true;
        out = static_cast<T>(static_cast<char>(tbl->GetField<std::int8_t>(slot, 0)));
        return true;
    }

    template <typename T>
    bool visit_bytes(T& out) {
        if(tbl == nullptr)
            return true;
        const auto* vec = tbl->GetPointer<const Vector<std::uint8_t>*>(slot);
        if(vec == nullptr)
            return true;
        if constexpr(std::same_as<T, std::vector<std::byte>>) {
            out.resize(vec->size());
            for(std::size_t i = 0; i < vec->size(); ++i) {
                out[i] = static_cast<std::byte>(vec->Get(static_cast<uoffset_t>(i)));
            }
        } else if constexpr(std::same_as<T, std::span<const std::byte>>) {
            out = std::span<const std::byte>(reinterpret_cast<const std::byte*>(vec->data()),
                                             vec->size());
        } else {
            auto data = reinterpret_cast<const std::byte*>(vec->data());
            out = T(data, data + vec->size());
        }
        return true;
    }

    template <typename T, typename Body>
    inline bool visit_struct(T& out, Body&& body);

    template <typename T, typename Body>
    inline bool visit_seq(T& out, Body&& body);

    template <typename T, typename Body>
    inline bool visit_tuple(T& out, Body&& body);

    template <typename T, typename Body>
    inline bool visit_map(T& out, Body&& body);

    template <typename Body>
    inline bool visit_variant(Body&& body);

private:
    const Table* follow_table() const {
        if(tbl == nullptr)
            return nullptr;
        if(slot == 0)
            return tbl;
        return tbl->GetPointer<const Table*>(slot);
    }
};

template <typename Idx, typename F>
bool table_field_reader::visit_field(Idx, std::string_view, F&& reader) {
    const voffset_t vid = detail::first_field + detail::field_step * static_cast<voffset_t>(Idx{});
    field_reader fr{.tbl = tbl, .slot = vid};
    return reader(fr);
}

template <typename F>
bool table_field_reader::visit_element(F&& reader) {
    const voffset_t vid = detail::first_field + detail::field_step * static_cast<voffset_t>(idx);
    field_reader fr{.tbl = tbl, .slot = vid};
    ++idx;
    return reader(fr);
}

struct root_reader : field_reader {
    root_reader(const Table* root) : field_reader{.tbl = root, .slot = detail::first_field} {}

    template <typename U, typename Body>
    bool visit_struct(U&, Body&& body) {
        table_field_reader tfr{.tbl = tbl};
        return body(tfr);
    }

    template <typename U, typename Body>
    bool visit_tuple(U&, Body&& body) {
        table_field_reader tfr{.tbl = tbl};
        return body(tfr);
    }

    template <typename Body>
    bool visit_variant(Body&& body) {
        if(tbl == nullptr) {
            return scoped_context<rich_error>::fail(rich_error("null variant table"));
        }
        auto tag = tbl->GetField<std::uint32_t>(detail::first_field, 0);
        auto index = static_cast<std::size_t>(tag);
        const voffset_t payload_slot = static_cast<voffset_t>(
            detail::first_field + detail::field_step * static_cast<voffset_t>(index + 1));
        field_reader pv{.tbl = tbl, .slot = payload_slot};
        return body(index, pv);
    }
};

template <typename T, typename Body>
bool field_reader::visit_struct(T& out, Body&& body) {
    using V = std::remove_const_t<T>;
    if constexpr(std::same_as<V, std::monostate>) {
        return true;
    } else if constexpr(can_inline_struct_v<V>) {
        const auto* ptr = tbl ? tbl->GetStruct<const V*>(slot) : nullptr;
        if(ptr != nullptr)
            out = *ptr;
        return true;
    } else {
        const auto* child = follow_table();
        if(child == nullptr)
            return true;
        table_field_reader tfr{.tbl = child};
        return body(tfr);
    }
}

template <typename T, typename Body>
bool field_reader::visit_seq([[maybe_unused]] T& out, Body&& body) {
    if(tbl == nullptr)
        return true;
    using V = std::remove_const_t<T>;
    using E = std::ranges::range_value_t<V>;
    const auto effective_slot = (slot == 0) ? detail::first_field : slot;
    using vr_t = vec_reader<E>;
    const auto* vec = tbl->GetPointer<typename vr_t::vec_ptr_t>(effective_slot);
    vr_t vr{.vec = vec};
    return body(vr);
}

template <typename T, typename Body>
bool field_reader::visit_tuple(T&, Body&& body) {
    const auto* child = follow_table();
    if(child == nullptr)
        return true;
    table_field_reader tfr{.tbl = child};
    return body(tfr);
}

template <typename T, typename Body>
bool field_reader::visit_map(T&, Body&& body) {
    if(tbl == nullptr)
        return true;
    const auto effective_slot = (slot == 0) ? detail::first_field : slot;
    const auto* vec = tbl->GetPointer<const Vector<table_offset_t>*>(effective_slot);
    map_reader mr{.vec = vec};
    return body(mr);
}

template <typename Body>
bool field_reader::visit_variant(Body&& body) {
    const auto* var_table = follow_table();
    if(var_table == nullptr) {
        return scoped_context<rich_error>::fail(rich_error("null variant table"));
    }
    auto tag = var_table->GetField<std::uint32_t>(detail::first_field, 0);
    auto index = static_cast<std::size_t>(tag);
    const voffset_t payload_slot = static_cast<voffset_t>(
        detail::first_field + detail::field_step * static_cast<voffset_t>(index + 1));
    field_reader pv{.tbl = var_table, .slot = payload_slot};
    return body(index, pv);
}

template <typename E>
template <typename F>
bool vec_reader<E>::visit_element(F&& reader) {
    using enum proxy_detail::element_layout;

    if constexpr(layout == boxed) {
        const auto* wrapper = vec->template GetAs<Table>(idx);
        ++idx;
        field_reader fr{.tbl = wrapper, .slot = detail::first_field};
        return reader(fr);
    } else if constexpr(layout == scalar) {
        auto val = vec->Get(idx);
        ++idx;
        scalar_reader<decltype(val)> sr{.value = val};
        return reader(sr);
    } else if constexpr(layout == string) {
        const auto* text = vec->GetAsString(static_cast<uoffset_t>(idx));
        ++idx;
        std::string_view sv;
        if(text != nullptr) {
            sv = std::string_view(text->data(), text->size());
        }
        string_reader sr{.value = sv};
        return reader(sr);
    } else if constexpr(layout == inline_struct) {
        const auto* ptr = vec->Get(static_cast<uoffset_t>(idx));
        ++idx;
        scalar_reader<repr_t> sr{.value = ptr ? *ptr : repr_t{}};
        return reader(sr);
    } else {
        const auto* child = vec->template GetAs<Table>(static_cast<uoffset_t>(idx));
        ++idx;
        field_reader fr{.tbl = child, .slot = 0};
        return reader(fr);
    }
}

template <typename KF, typename VF>
bool map_reader::visit_entry(KF&& key_fn, VF&& val_fn) {
    const auto* entry = vec->template GetAs<Table>(idx);
    ++idx;
    if(entry == nullptr) {
        return scoped_context<rich_error>::fail(rich_error("null map entry table"));
    }
    field_reader kr{.tbl = entry, .slot = detail::first_field};
    KOTA_CODEC_TRY(key_fn(kr));
    field_reader vr{.tbl = entry, .slot = detail::first_field + detail::field_step};
    return val_fn(vr);
}

}  // namespace decode_detail

template <typename Config = void, typename T>
auto from_flatbuffer(std::span<const std::byte> buf, T& out) -> std::expected<void, rich_error> {
    if(buf.empty()) {
        return std::unexpected(rich_error("empty buffer"));
    }

    const auto* data = reinterpret_cast<const std::uint8_t*>(buf.data());
    auto size = buf.size();

    if(!::flatbuffers::BufferHasIdentifier(data, detail::buffer_identifier)) {
        return std::unexpected(rich_error("invalid buffer identifier"));
    }

    const auto* root = ::flatbuffers::GetRoot<Table>(data);
    if(root == nullptr) {
        return std::unexpected(rich_error("null root table"));
    }

    verifier_t verifier(data, size);
    if(!root->VerifyTableStart(verifier) || !verifier.EndTable()) {
        return std::unexpected(rich_error("buffer verification failed"));
    }

    rich_error err;
    scoped_context<rich_error> guard(err);

    decode_detail::root_reader vis(root);
    if(!decode_value<default_config<Config>>(vis, out)) {
        return std::unexpected(std::move(err));
    }
    return {};
}

template <typename Config = void, typename T>
auto from_flatbuffer(std::span<const std::uint8_t> buf, T& out) -> std::expected<void, rich_error> {
    auto bytes =
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(buf.data()), buf.size());
    return from_flatbuffer<Config>(bytes, out);
}

template <typename T, typename Config = void>
    requires std::default_initializable<T>
auto from_flatbuffer(std::span<const std::uint8_t> buf) -> std::expected<T, rich_error> {
    T value{};
    auto result = from_flatbuffer<Config>(buf, value);
    if(!result) {
        return std::unexpected(result.error());
    }
    return value;
}

template <typename T, typename Config = void>
    requires std::default_initializable<T>
auto from_flatbuffer(std::span<const std::byte> buf) -> std::expected<T, rich_error> {
    T value{};
    auto result = from_flatbuffer<Config>(buf, value);
    if(!result) {
        return std::unexpected(result.error());
    }
    return value;
}

}  // namespace kota::codec::fbs
