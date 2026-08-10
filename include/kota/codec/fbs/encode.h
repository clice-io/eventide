#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "kota/support/ranges.h"
#include "kota/meta/repr.h"
#include "kota/meta/type_kind.h"
#include "kota/codec/fbs/proxy.h"
#include "kota/codec/fbs/type.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/encode.h"

namespace kota::codec::fbs {

namespace encode_detail {

using fbs::builder_t;
using fbs::table_offset_t;
using proxy_detail::slot_id;

struct alloc_field_visitor;
struct alloc_table_visitor;
struct write_field_visitor;
struct write_table_visitor;

template <typename T>
struct scalar_elem_visitor;
template <typename T>
struct scalar_collector;

struct string_elem_visitor;
struct string_collector;

template <typename T>
struct inline_struct_elem_visitor;
template <typename T>
struct inline_struct_collector;

struct table_elem_visitor;
struct table_collector;
struct boxed_table_collector;

template <typename Key>
struct map_entry_collector;
template <typename Key>
struct key_capture_visitor;

struct root_visitor;

template <typename Body>
inline auto two_pass(builder_t& fbb, Body&& body) -> table_offset_t;

struct alloc_field_visitor : detail::visitor_base {
    builder_t& fbb;
    uoffset_t stored_offset = 0;

    bool visit_bool(bool) {
        return true;
    }

    template <typename T>
    bool visit_int(T) {
        return true;
    }

    template <typename T>
    bool visit_uint(T) {
        return true;
    }

    template <typename T>
    bool visit_float(T) {
        return true;
    }

    template <typename T>
    bool visit_char(T) {
        return true;
    }

    bool visit_null() {
        return true;
    }

    template <typename T>
    bool visit_str(const T& v) {
        std::string_view sv(v);
        auto off = fbb.CreateString(sv.data(), sv.size());
        stored_offset = off.o;
        return true;
    }

    template <typename T>
    bool visit_bytes(const T& v) {
        auto data = reinterpret_cast<const std::uint8_t*>(std::data(v));
        auto len = std::size(v);
        auto off = fbb.CreateVector(data, len);
        stored_offset = off.o;
        return true;
    }

    template <typename T, typename Body>
    inline bool visit_struct(const T&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_seq(const Container& c, Body&& body);

    template <typename T, typename Body>
    inline bool visit_tuple(const T&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_map(const Container& m, Body&& body);

    template <typename Body>
    inline bool visit_variant(std::size_t index, Body&& body);
};

struct alloc_table_visitor : detail::visitor_base {
    builder_t& fbb;
    std::vector<uoffset_t> offsets{};
    std::size_t next_idx = 0;

    template <typename F>
    bool visit_field(auto index, std::string_view /*name*/, F&& writer) {
        const std::size_t I = index;
        if(offsets.size() <= I) {
            offsets.resize(I + 1, 0);
        }
        alloc_field_visitor fv{.fbb = fbb};
        KOTA_CODEC_TRY(writer(fv));
        offsets[I] = fv.stored_offset;
        return true;
    }

    template <typename F>
    bool visit_element(F&& writer) {
        alloc_field_visitor fv{.fbb = fbb};
        KOTA_CODEC_TRY(writer(fv));
        if(offsets.size() <= next_idx) {
            offsets.resize(next_idx + 1, 0);
        }
        offsets[next_idx] = fv.stored_offset;
        ++next_idx;
        return true;
    }
};

struct write_field_visitor : detail::visitor_base {
    builder_t& fbb;
    slot_id sid;
    uoffset_t stored_offset = 0;

    bool visit_bool(bool v) {
        fbb.AddElement<std::uint8_t>(sid, static_cast<std::uint8_t>(v));
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        fbb.AddElement<T>(sid, v);
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        fbb.AddElement<T>(sid, v);
        return true;
    }

    template <typename T>
    bool visit_float(T v) {
        // long double fields store as double cells, matching the decode side.
        using cell_t = proxy_detail::scalar_cell_t<T>;
        fbb.AddElement<cell_t>(sid, static_cast<cell_t>(v));
        return true;
    }

    template <typename T>
    bool visit_char(T v) {
        fbb.AddElement<std::int8_t>(sid, static_cast<std::int8_t>(v));
        return true;
    }

    bool visit_null() {
        return true;
    }

    template <typename T>
    bool visit_str(const T&) {
        fbb.AddOffset(sid, offset_t<void>(stored_offset));
        return true;
    }

    template <typename T>
    bool visit_bytes(const T&) {
        fbb.AddOffset(sid, offset_t<void>(stored_offset));
        return true;
    }

    template <typename T, typename Body>
    bool visit_struct(const T& value, Body&&) {
        if constexpr(can_inline_struct_v<T>) {
            fbb.AddStruct(sid, &value);
        } else {
            fbb.AddOffset(sid, offset_t<void>(stored_offset));
        }
        return true;
    }

    template <typename Container, typename Body>
    bool visit_seq(const Container&, Body&&) {
        fbb.AddOffset(sid, offset_t<void>(stored_offset));
        return true;
    }

    template <typename T, typename Body>
    bool visit_tuple(const T&, Body&&) {
        fbb.AddOffset(sid, offset_t<void>(stored_offset));
        return true;
    }

    template <typename Container, typename Body>
    bool visit_map(const Container&, Body&&) {
        fbb.AddOffset(sid, offset_t<void>(stored_offset));
        return true;
    }

    template <typename Body>
    bool visit_variant(std::size_t, Body&&) {
        fbb.AddOffset(sid, offset_t<void>(stored_offset));
        return true;
    }
};

struct write_table_visitor : detail::visitor_base {
    builder_t& fbb;
    const std::vector<uoffset_t>& offsets;
    std::size_t next_idx = 0;

    template <typename F>
    bool visit_field(auto index, std::string_view /*name*/, F&& writer) {
        const std::size_t I = index;
        const slot_id sid = detail::first_field + detail::field_step * static_cast<slot_id>(I);
        const auto off = (I < offsets.size()) ? offsets[I] : uoffset_t{0};
        write_field_visitor wv{.fbb = fbb, .sid = sid, .stored_offset = off};
        return writer(wv);
    }

    template <typename F>
    bool visit_element(F&& writer) {
        const slot_id sid =
            detail::first_field + detail::field_step * static_cast<slot_id>(next_idx);
        const auto off = (next_idx < offsets.size()) ? offsets[next_idx] : uoffset_t{0};
        write_field_visitor wv{.fbb = fbb, .sid = sid, .stored_offset = off};
        ++next_idx;
        return writer(wv);
    }
};

template <typename T>
struct scalar_elem_visitor : detail::visitor_base {
    std::vector<T>& elems;

    bool visit_bool(bool v) {
        elems.push_back(static_cast<T>(v));
        return true;
    }

    template <typename U>
    bool visit_int(U v) {
        elems.push_back(static_cast<T>(v));
        return true;
    }

    template <typename U>
    bool visit_uint(U v) {
        elems.push_back(static_cast<T>(v));
        return true;
    }

    template <typename U>
    bool visit_float(U v) {
        elems.push_back(static_cast<T>(v));
        return true;
    }

    template <typename U>
    bool visit_char(U v) {
        elems.push_back(static_cast<T>(v));
        return true;
    }
};

template <typename T>
struct scalar_collector {
    builder_t& fbb;
    std::vector<T> elems{};
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        scalar_elem_visitor<T> ev{.elems = elems};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVector(elems.data(), elems.size());
        result_offset = off.o;
        return true;
    }
};

struct string_elem_visitor : detail::visitor_base {
    builder_t& fbb;
    std::vector<string_offset_t>& refs;

    template <typename T>
    bool visit_str(const T& v) {
        std::string_view sv(v);
        refs.push_back(fbb.CreateString(sv.data(), sv.size()));
        return true;
    }
};

struct string_collector {
    builder_t& fbb;
    std::vector<string_offset_t> refs{};
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        string_elem_visitor ev{.fbb = fbb, .refs = refs};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVector(refs.data(), refs.size());
        result_offset = off.o;
        return true;
    }
};

template <typename T>
struct inline_struct_elem_visitor : detail::visitor_base {
    std::vector<T>& elems;

    // The dispatch calls visit_struct for structures. For inline structs the
    // value is simply copied into the element vector.
    template <typename U, typename Body>
    bool visit_struct(const U& value, Body&&) {
        static_assert(std::is_same_v<U, T>);
        elems.push_back(value);
        return true;
    }
};

template <typename T>
struct inline_struct_collector {
    builder_t& fbb;
    std::vector<T> elems{};
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        inline_struct_elem_visitor<T> ev{.elems = elems};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVectorOfStructs(elems.data(), elems.size());
        result_offset = off.o;
        return true;
    }
};

struct table_elem_visitor : detail::visitor_base {
    builder_t& fbb;
    std::vector<table_offset_t>& table_offsets;

    // Scalar no-ops — should not be reached for table elements, but provide
    // stubs to satisfy the visitor concept in edge cases (e.g. variant
    // alternatives that are scalars).
    bool visit_bool(bool) {
        return true;
    }

    template <typename T>
    bool visit_int(T) {
        return true;
    }

    template <typename T>
    bool visit_uint(T) {
        return true;
    }

    template <typename T>
    bool visit_float(T) {
        return true;
    }

    template <typename T>
    bool visit_char(T) {
        return true;
    }

    bool visit_null() {
        return true;
    }

    template <typename T>
    bool visit_str(const T&) {
        return true;
    }

    template <typename T>
    bool visit_bytes(const T&) {
        return true;
    }

    template <typename T, typename Body>
    inline bool visit_struct(const T&, Body&& body);

    template <typename T, typename Body>
    inline bool visit_tuple(const T&, Body&& body);

    template <typename Body>
    inline bool visit_variant(std::size_t index, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_seq(const Container& c, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_map(const Container& m, Body&& body);
};

struct table_collector {
    builder_t& fbb;
    std::vector<table_offset_t> table_offsets{};
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        table_elem_visitor ev{.fbb = fbb, .table_offsets = table_offsets};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVector(table_offsets.data(), table_offsets.size());
        result_offset = off.o;
        return true;
    }
};

struct boxed_table_collector {
    builder_t& fbb;
    std::vector<table_offset_t> table_offsets{};
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        alloc_field_visitor av{.fbb = fbb};
        KOTA_CODEC_TRY(writer(av));

        auto start = fbb.StartTable();
        write_field_visitor wv{.fbb = fbb,
                               .sid = detail::first_field,
                               .stored_offset = av.stored_offset};
        KOTA_CODEC_TRY(writer(wv));
        table_offsets.push_back(table_offset_t(fbb.EndTable(start)));
        return true;
    }

    bool finish() {
        auto off = fbb.CreateVector(table_offsets.data(), table_offsets.size());
        result_offset = off.o;
        return true;
    }
};

/// The owning key an encoded map entry is ordered by, matching the comparison
/// map_view::find_entry applies to decoded keys: strings compare
/// lexicographically (captured owning — the buffer is still being built),
/// enums by their underlying value, every other scalar as itself.
template <typename K>
constexpr auto ordering_key_impl() {
    using clean_k = proxy_detail::deep_clean_t<K>;
    if constexpr(proxy_detail::is_string_like_v<clean_k>) {
        return std::type_identity<std::string>{};
    } else if constexpr(std::is_enum_v<clean_k>) {
        return std::type_identity<std::underlying_type_t<clean_k>>{};
    } else {
        return std::type_identity<clean_k>{};
    }
}

template <typename K>
using ordering_key_t = typename decltype(ordering_key_impl<K>())::type;

/// Captures a map key as the ordering key its entry is sorted by. The events
/// mirror how the key's resolved representation encodes: scalar keys fire
/// exactly one scalar event, string keys fire visit_str. The visit_str guard
/// exists because configs spelling non-finite floats as strings
/// (nan_repr::String) instantiate visit_str for scalar keys too.
template <typename Key>
struct key_capture_visitor : detail::visitor_base {
    Key captured{};

    bool visit_bool(bool v) {
        return store(v);
    }

    template <typename T>
    bool visit_int(T v) {
        return store(v);
    }

    template <typename T>
    bool visit_uint(T v) {
        return store(v);
    }

    template <typename T>
    bool visit_float(T v) {
        return store(v);
    }

    template <typename T>
    bool visit_char(T v) {
        return store(v);
    }

    template <typename T>
    bool visit_str(const T& v) {
        if constexpr(std::same_as<Key, std::string>) {
            captured = std::string(std::string_view(v));
        }
        return true;
    }

private:
    template <typename T>
    bool store(T v) {
        captured = static_cast<Key>(v);
        return true;
    }
};

template <typename Key>
struct map_entry_collector {
    builder_t& fbb;
    std::vector<std::pair<Key, table_offset_t>> entries{};

    template <typename KF, typename VF>
    inline bool visit_entry(KF&& key_fn, VF&& value_fn);
};

struct root_visitor : detail::visitor_base {
    builder_t& fbb;
    table_offset_t root_off{0};

    bool visit_bool(bool v) {
        return box_root_scalar<std::uint8_t>(static_cast<std::uint8_t>(v));
    }

    template <typename T>
    bool visit_int(T v) {
        return box_root_scalar<T>(v);
    }

    template <typename T>
    bool visit_uint(T v) {
        return box_root_scalar<T>(v);
    }

    template <typename T>
    bool visit_float(T v) {
        using cell_t = proxy_detail::scalar_cell_t<T>;
        return box_root_scalar<cell_t>(static_cast<cell_t>(v));
    }

    template <typename T>
    bool visit_char(T v) {
        return box_root_scalar<std::int8_t>(static_cast<std::int8_t>(v));
    }

    template <typename T>
    bool visit_str(const T& v) {
        std::string_view sv(v);
        auto str_off = fbb.CreateString(sv.data(), sv.size());
        auto start = fbb.StartTable();
        fbb.AddOffset(detail::first_field, str_off);
        root_off = table_offset_t(fbb.EndTable(start));
        return true;
    }

    template <typename T>
    bool visit_bytes(const T& v) {
        auto data = reinterpret_cast<const std::uint8_t*>(std::data(v));
        auto len = std::size(v);
        auto vec_off = fbb.CreateVector(data, len);
        auto start = fbb.StartTable();
        fbb.AddOffset(detail::first_field, vec_off);
        root_off = table_offset_t(fbb.EndTable(start));
        return true;
    }

    bool visit_null() {
        auto start = fbb.StartTable();
        root_off = table_offset_t(fbb.EndTable(start));
        return true;
    }

    template <typename T, typename Body>
    inline bool visit_struct(const T&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_seq(const Container& c, Body&& body);

    template <typename T, typename Body>
    inline bool visit_tuple(const T&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_map(const Container& m, Body&& body);

    template <typename Body>
    inline bool visit_variant(std::size_t index, Body&& body);

private:
    template <typename T>
    bool box_root_scalar(T v) {
        auto start = fbb.StartTable();
        fbb.AddElement<T>(detail::first_field, v);
        root_off = table_offset_t(fbb.EndTable(start));
        return true;
    }
};

template <typename Body>
auto two_pass(builder_t& fbb, Body&& body) -> table_offset_t {
    alloc_table_visitor av{.fbb = fbb};
    if(!body(av))
        return table_offset_t{0};

    auto start = fbb.StartTable();
    write_table_visitor wv{.fbb = fbb, .offsets = av.offsets};
    if(!body(wv)) {
        fbb.EndTable(start);
        return table_offset_t{0};
    }
    return table_offset_t(fbb.EndTable(start));
}

/// The declared key type of a map-kind container.
template <typename Container>
using map_key_t = kota::map_entry_key_t<std::ranges::range_value_t<Container>>;

template <typename Key, typename Body>
inline bool encode_sorted_map(builder_t& fbb, Body&& body, uoffset_t& out_offset) {
    map_entry_collector<ordering_key_t<Key>> coll{.fbb = fbb};
    KOTA_CODEC_TRY(body(coll));

    std::sort(coll.entries.begin(), coll.entries.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<table_offset_t> sorted_offsets;
    sorted_offsets.reserve(coll.entries.size());
    for(auto& entry: coll.entries) {
        sorted_offsets.push_back(entry.second);
    }
    out_offset = fbb.CreateVector(sorted_offsets.data(), sorted_offsets.size()).o;
    return true;
}

template <typename Body>
inline bool
    encode_variant_table(builder_t& fbb, std::size_t index, Body&& body, uoffset_t& out_offset) {
    alloc_field_visitor payload_alloc{.fbb = fbb};
    KOTA_CODEC_TRY(body(payload_alloc));

    const slot_id payload_slot =
        detail::first_field + detail::field_step * static_cast<slot_id>(index + 1);

    auto start = fbb.StartTable();
    fbb.AddElement<std::uint32_t>(detail::first_field, static_cast<std::uint32_t>(index));
    write_field_visitor payload_write{.fbb = fbb,
                                      .sid = payload_slot,
                                      .stored_offset = payload_alloc.stored_offset};
    body(payload_write);
    out_offset = fbb.EndTable(start);
    return true;
}

template <typename Seq>
using element_clean_t = std::remove_cvref_t<std::ranges::range_value_t<Seq>>;

template <typename Container, typename Body>
bool seq_encode_impl(builder_t& fbb, const Container& c, Body&& body, uoffset_t& out_offset) {
    using enum proxy_detail::element_layout;
    using element_t = element_clean_t<Container>;
    using repr_t = proxy_detail::apply_repr_t<element_t>;
    constexpr auto layout = proxy_detail::element_layout_of<element_t>();

    // A representation differing from the element type converts per element
    // in the visit chain, so the contiguous fast paths apply only when
    // elements travel as themselves.
    constexpr bool identity = std::same_as<repr_t, element_t>;
    constexpr bool contiguous =
        std::ranges::contiguous_range<Container> && std::ranges::sized_range<Container>;

    auto collect = [&](auto coll) -> bool {
        KOTA_CODEC_TRY(body(coll));
        KOTA_CODEC_TRY(coll.finish());
        out_offset = coll.result_offset;
        return true;
    };

    if constexpr(layout == scalar) {
        using cell_t = proxy_detail::scalar_cell_t<repr_t>;
        // Bitwise cell-compatible elements (the cell type itself, plus bool
        // and std::byte, whose cells share their object representation) can
        // be written straight from a contiguous range.
        constexpr bool cell_compatible = std::same_as<element_t, cell_t> ||
                                         std::same_as<element_t, std::byte> ||
                                         (std::same_as<element_t, bool> && sizeof(bool) == 1);
        if constexpr(identity && contiguous && cell_compatible) {
            auto data = reinterpret_cast<const cell_t*>(std::ranges::data(c));
            out_offset = fbb.CreateVector(data, std::ranges::size(c)).o;
            return true;
        } else {
            return collect(scalar_collector<cell_t>{.fbb = fbb});
        }
    } else if constexpr(layout == string) {
        return collect(string_collector{.fbb = fbb});
    } else if constexpr(layout == boxed) {
        return collect(boxed_table_collector{.fbb = fbb});
    } else if constexpr(layout == inline_struct) {
        if constexpr(identity && contiguous) {
            out_offset = fbb.CreateVectorOfStructs(std::ranges::data(c), std::ranges::size(c)).o;
            return true;
        } else {
            return collect(inline_struct_collector<repr_t>{.fbb = fbb});
        }
    } else {
        return collect(table_collector{.fbb = fbb});
    }
}

template <typename T, typename Body>
bool alloc_field_visitor::visit_struct(const T&, Body&& body) {
    if constexpr(can_inline_struct_v<T>) {
        // Inline structs are written directly by write_field_visitor::visit_struct.
        // No allocation needed.
        return true;
    } else {
        auto off = two_pass(fbb, std::forward<Body>(body));
        stored_offset = off.o;
        return true;
    }
}

template <typename Container, typename Body>
bool alloc_field_visitor::visit_seq(const Container& c, Body&& body) {
    return seq_encode_impl(fbb, c, std::forward<Body>(body), stored_offset);
}

template <typename T, typename Body>
bool alloc_field_visitor::visit_tuple(const T&, Body&& body) {
    auto off = two_pass(fbb, std::forward<Body>(body));
    stored_offset = off.o;
    return true;
}

template <typename Container, typename Body>
bool alloc_field_visitor::visit_map(const Container&, Body&& body) {
    return encode_sorted_map<map_key_t<Container>>(fbb, std::forward<Body>(body), stored_offset);
}

template <typename Body>
bool alloc_field_visitor::visit_variant(std::size_t index, Body&& body) {
    return encode_variant_table(fbb, index, std::forward<Body>(body), stored_offset);
}

template <typename T, typename Body>
bool table_elem_visitor::visit_struct(const T&, Body&& body) {
    auto off = two_pass(fbb, std::forward<Body>(body));
    table_offsets.push_back(off);
    return true;
}

template <typename T, typename Body>
bool table_elem_visitor::visit_tuple(const T&, Body&& body) {
    auto off = two_pass(fbb, std::forward<Body>(body));
    table_offsets.push_back(off);
    return true;
}

template <typename Body>
bool table_elem_visitor::visit_variant(std::size_t index, Body&& body) {
    uoffset_t off = 0;
    KOTA_CODEC_TRY(encode_variant_table(fbb, index, std::forward<Body>(body), off));
    table_offsets.push_back(table_offset_t(off));
    return true;
}

template <typename Container, typename Body>
bool table_elem_visitor::visit_seq(const Container& c, Body&& body) {
    uoffset_t inner_offset = 0;
    KOTA_CODEC_TRY(seq_encode_impl(fbb, c, std::forward<Body>(body), inner_offset));

    auto start = fbb.StartTable();
    fbb.AddOffset(detail::first_field, offset_t<void>(inner_offset));
    table_offsets.push_back(table_offset_t(fbb.EndTable(start)));
    return true;
}

template <typename Container, typename Body>
bool table_elem_visitor::visit_map(const Container&, Body&& body) {
    uoffset_t vec_off = 0;
    KOTA_CODEC_TRY(encode_sorted_map<map_key_t<Container>>(fbb, std::forward<Body>(body), vec_off));

    auto start = fbb.StartTable();
    fbb.AddOffset(detail::first_field, offset_t<void>(vec_off));
    table_offsets.push_back(table_offset_t(fbb.EndTable(start)));
    return true;
}

template <typename Key>
template <typename KF, typename VF>
bool map_entry_collector<Key>::visit_entry(KF&& key_fn, VF&& value_fn) {
    key_capture_visitor<Key> capture;
    KOTA_CODEC_TRY(key_fn(capture));

    auto table_off = two_pass(fbb, [&](auto& sv) -> bool {
        KOTA_CODEC_TRY(sv.visit_field(std::integral_constant<std::size_t, 0>{},
                                      std::string_view{"key"},
                                      [&](auto& kv) -> bool { return key_fn(kv); }));
        KOTA_CODEC_TRY(sv.visit_field(std::integral_constant<std::size_t, 1>{},
                                      std::string_view{"value"},
                                      [&](auto& vv) -> bool { return value_fn(vv); }));
        return true;
    });

    entries.emplace_back(std::move(capture.captured), table_off);
    return true;
}

template <typename T, typename Body>
bool root_visitor::visit_struct(const T&, Body&& body) {
    root_off = two_pass(fbb, std::forward<Body>(body));
    return true;
}

template <typename Container, typename Body>
bool root_visitor::visit_seq(const Container& c, Body&& body) {
    uoffset_t inner_offset = 0;
    KOTA_CODEC_TRY(seq_encode_impl(fbb, c, std::forward<Body>(body), inner_offset));

    auto start = fbb.StartTable();
    fbb.AddOffset(detail::first_field, offset_t<void>(inner_offset));
    root_off = table_offset_t(fbb.EndTable(start));
    return true;
}

template <typename T, typename Body>
bool root_visitor::visit_tuple(const T&, Body&& body) {
    root_off = two_pass(fbb, std::forward<Body>(body));
    return true;
}

template <typename Container, typename Body>
bool root_visitor::visit_map(const Container&, Body&& body) {
    uoffset_t vec_off = 0;
    KOTA_CODEC_TRY(encode_sorted_map<map_key_t<Container>>(fbb, std::forward<Body>(body), vec_off));

    auto start = fbb.StartTable();
    fbb.AddOffset(detail::first_field, offset_t<void>(vec_off));
    root_off = table_offset_t(fbb.EndTable(start));
    return true;
}

template <typename Body>
bool root_visitor::visit_variant(std::size_t index, Body&& body) {
    uoffset_t off = 0;
    KOTA_CODEC_TRY(encode_variant_table(fbb, index, std::forward<Body>(body), off));
    root_off = table_offset_t(off);
    return true;
}

}  // namespace encode_detail

template <typename Config = void, typename T>
auto to_flatbuffer(const T& value, std::optional<std::size_t> initial_capacity = std::nullopt)
    -> std::expected<std::vector<std::uint8_t>, rich_error> {
    rich_error err;
    scoped_context<rich_error> guard(err);

    encode_detail::builder_t fbb(initial_capacity.value_or(1024));
    encode_detail::root_visitor vis{.fbb = fbb};

    if(!encode_value<default_config<Config>>(vis, value)) {
        return std::unexpected(std::move(err));
    }

    fbb.Finish(vis.root_off, detail::buffer_identifier);
    const auto* begin = fbb.GetBufferPointer();
    return std::vector<std::uint8_t>(begin, begin + fbb.GetSize());
}

}  // namespace kota::codec::fbs

namespace kota::codec {

// std::monostate is reflectable_class (aggregate with 0 fields), so the old
// arena encoder wrote it as an empty table.  The decoder expects a table
// reference at the payload slot, so we must match that layout.
template <typename Config>
struct serialize_visit<fbs::encode_detail::alloc_field_visitor, std::monostate, Config> {
    static bool visit(fbs::encode_detail::alloc_field_visitor& vis, const std::monostate&) {
        auto start = vis.fbb.StartTable();
        vis.stored_offset = vis.fbb.EndTable(start);
        return true;
    }
};

template <typename Config>
struct serialize_visit<fbs::encode_detail::write_field_visitor, std::monostate, Config> {
    static bool visit(fbs::encode_detail::write_field_visitor& vis, const std::monostate&) {
        vis.fbb.AddOffset(vis.sid, fbs::offset_t<void>(vis.stored_offset));
        return true;
    }
};

template <typename Config>
struct serialize_visit<fbs::encode_detail::root_visitor, std::monostate, Config> {
    static bool visit(fbs::encode_detail::root_visitor& vis, const std::monostate&) {
        return vis.visit_null();
    }
};

template <typename Config>
struct serialize_visit<fbs::encode_detail::table_elem_visitor, std::monostate, Config> {
    static bool visit(fbs::encode_detail::table_elem_visitor& vis, const std::monostate&) {
        auto start = vis.fbb.StartTable();
        vis.table_offsets.push_back(fbs::encode_detail::table_offset_t(vis.fbb.EndTable(start)));
        return true;
    }
};

}  // namespace kota::codec
