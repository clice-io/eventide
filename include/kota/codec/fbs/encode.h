#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "kota/meta/type_kind.h"
#include "kota/codec/fbs/type.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/encode.h"

namespace kota::codec::fbs {

namespace encode_detail {

using fbs::builder_t;
using fbs::table_offset_t;
using slot_id_t = voffset_t;

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

struct byte_collector;

struct map_entry_collector;
struct key_capture_visitor;

struct root_visitor;

template <typename Body>
inline auto two_pass(builder_t& fbb, Body&& body) -> table_offset_t;

struct alloc_field_visitor {
    builder_t& fbb;
    uoffset_t stored_offset = 0;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

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

struct alloc_table_visitor {
    builder_t& fbb;
    std::vector<uoffset_t> offsets;
    std::size_t next_idx = 0;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

    template <typename F>
    bool visit_field(auto index, std::string_view /*name*/, F&& writer) {
        const std::size_t I = index;
        if(offsets.size() <= I) {
            offsets.resize(I + 1, 0);
        }
        alloc_field_visitor fv{fbb};
        KOTA_CODEC_TRY(writer(fv));
        offsets[I] = fv.stored_offset;
        return true;
    }

    template <typename F>
    bool visit_element(F&& writer) {
        alloc_field_visitor fv{fbb};
        KOTA_CODEC_TRY(writer(fv));
        if(offsets.size() <= next_idx) {
            offsets.resize(next_idx + 1, 0);
        }
        offsets[next_idx] = fv.stored_offset;
        ++next_idx;
        return true;
    }
};

struct write_field_visitor {
    builder_t& fbb;
    slot_id_t sid;
    uoffset_t stored_offset;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

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
        fbb.AddElement<T>(sid, v);
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

struct write_table_visitor {
    builder_t& fbb;
    const std::vector<uoffset_t>& offsets;
    std::size_t next_idx = 0;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

    template <typename F>
    bool visit_field(auto index, std::string_view /*name*/, F&& writer) {
        const std::size_t I = index;
        const slot_id_t sid = detail::first_field + detail::field_step * static_cast<slot_id_t>(I);
        const auto off = (I < offsets.size()) ? offsets[I] : uoffset_t{0};
        write_field_visitor wv{fbb, sid, off};
        return writer(wv);
    }

    template <typename F>
    bool visit_element(F&& writer) {
        const slot_id_t sid =
            detail::first_field + detail::field_step * static_cast<slot_id_t>(next_idx);
        const auto off = (next_idx < offsets.size()) ? offsets[next_idx] : uoffset_t{0};
        write_field_visitor wv{fbb, sid, off};
        ++next_idx;
        return writer(wv);
    }
};

template <typename T>
struct scalar_elem_visitor {
    std::vector<T>& elems;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

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
    std::vector<T> elems;
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        scalar_elem_visitor<T> ev{elems};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVector(elems.data(), elems.size());
        result_offset = off.o;
        return true;
    }
};

struct string_elem_visitor {
    builder_t& fbb;
    std::vector<string_offset_t>& refs;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

    template <typename T>
    bool visit_str(const T& v) {
        std::string_view sv(v);
        refs.push_back(fbb.CreateString(sv.data(), sv.size()));
        return true;
    }
};

struct string_collector {
    builder_t& fbb;
    std::vector<string_offset_t> refs;
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        string_elem_visitor ev{fbb, refs};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVector(refs.data(), refs.size());
        result_offset = off.o;
        return true;
    }
};

template <typename T>
struct inline_struct_elem_visitor {
    std::vector<T>& elems;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

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
    std::vector<T> elems;
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        inline_struct_elem_visitor<T> ev{elems};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVectorOfStructs(elems.data(), elems.size());
        result_offset = off.o;
        return true;
    }
};

struct table_elem_visitor {
    builder_t& fbb;
    std::vector<table_offset_t>& table_offsets;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

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
    std::vector<table_offset_t> table_offsets;
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        table_elem_visitor ev{fbb, table_offsets};
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
    std::vector<table_offset_t> table_offsets;
    uoffset_t result_offset = 0;

    template <typename F>
    bool visit_element(F&& writer) {
        alloc_field_visitor av{fbb, 0};
        KOTA_CODEC_TRY(writer(av));

        auto start = fbb.StartTable();
        write_field_visitor wv{fbb, detail::first_field, av.stored_offset};
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

struct byte_collector {
    builder_t& fbb;
    std::vector<std::uint8_t> bytes;
    uoffset_t result_offset = 0;

    // The dispatch sends bytes through visit_int (uint8) or visit_bytes,
    // but for sequence element encoding it arrives through the element
    // writer body.  We capture via a small inner visitor.
    struct elem_vis {
        std::vector<std::uint8_t>& bytes;
        using error_type = rich_error;
        constexpr static bool human_readable = false;

        template <typename T>
        bool visit_int(T v) {
            bytes.push_back(static_cast<std::uint8_t>(v));
            return true;
        }

        template <typename T>
        bool visit_uint(T v) {
            bytes.push_back(static_cast<std::uint8_t>(v));
            return true;
        }
    };

    template <typename F>
    bool visit_element(F&& writer) {
        elem_vis ev{bytes};
        return writer(ev);
    }

    bool finish() {
        auto off = fbb.CreateVector(bytes.data(), bytes.size());
        result_offset = off.o;
        return true;
    }
};

struct key_capture_visitor {
    std::string captured;

    using error_type = rich_error;
    constexpr static bool human_readable = false;

    bool visit_bool(bool v) {
        captured = v ? "true" : "false";
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        captured = std::to_string(static_cast<std::int64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        captured = std::to_string(static_cast<std::uint64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_float(T v) {
        captured = std::to_string(static_cast<double>(v));
        return true;
    }

    template <typename T>
    bool visit_str(const T& v) {
        captured = std::string(std::string_view(v));
        return true;
    }

    template <typename T>
    bool visit_char(T v) {
        captured = std::string(1, static_cast<char>(v));
        return true;
    }
};

struct map_entry_collector {
    builder_t& fbb;
    std::vector<std::pair<std::string, table_offset_t>> entries;

    template <typename KF, typename VF>
    inline bool visit_entry(KF&& key_fn, VF&& value_fn);
};

struct root_visitor {
    builder_t& fbb;
    table_offset_t root_off{0};

    using error_type = rich_error;
    constexpr static bool human_readable = false;

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
        return box_root_scalar<T>(v);
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
    alloc_table_visitor av{fbb, {}, 0};
    if(!body(av))
        return table_offset_t{0};

    auto start = fbb.StartTable();
    write_table_visitor wv{fbb, av.offsets, 0};
    if(!body(wv)) {
        fbb.EndTable(start);
        return table_offset_t{0};
    }
    return table_offset_t(fbb.EndTable(start));
}

template <typename Body>
inline bool encode_sorted_map(builder_t& fbb, Body&& body, uoffset_t& out_offset) {
    map_entry_collector coll{fbb, {}};
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
    alloc_field_visitor payload_alloc{fbb, 0};
    KOTA_CODEC_TRY(body(payload_alloc));

    const slot_id_t payload_slot =
        detail::first_field + detail::field_step * static_cast<slot_id_t>(index + 1);

    auto start = fbb.StartTable();
    fbb.AddElement<std::uint32_t>(detail::first_field, static_cast<std::uint32_t>(index));
    write_field_visitor payload_write{fbb, payload_slot, payload_alloc.stored_offset};
    body(payload_write);
    out_offset = fbb.EndTable(start);
    return true;
}

template <typename Seq>
using element_clean_t = std::remove_cvref_t<std::ranges::range_value_t<Seq>>;

template <typename Container, typename Body>
bool seq_encode_impl(builder_t& fbb, const Container& c, Body&& body, uoffset_t& out_offset) {
    using element_t = element_clean_t<Container>;

    if constexpr(std::same_as<element_t, std::byte>) {
        if constexpr(std::ranges::contiguous_range<Container> &&
                     std::ranges::sized_range<Container>) {
            auto data = reinterpret_cast<const std::uint8_t*>(std::ranges::data(c));
            auto len = std::ranges::size(c);
            auto off = fbb.CreateVector(data, len);
            out_offset = off.o;
            return true;
        } else {
            byte_collector coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        }
    } else if constexpr(meta::bool_like<element_t>) {
        if constexpr(std::ranges::contiguous_range<Container> &&
                     std::ranges::sized_range<Container> &&
                     sizeof(element_t) == sizeof(std::uint8_t)) {
            auto data = reinterpret_cast<const std::uint8_t*>(std::ranges::data(c));
            auto len = std::ranges::size(c);
            auto off = fbb.CreateVector(data, len);
            out_offset = off.o;
            return true;
        } else {
            scalar_collector<std::uint8_t> coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        }
    } else if constexpr(meta::int_like<element_t> || meta::uint_like<element_t> ||
                        meta::floating_like<element_t> || meta::char_like<element_t>) {
        using wire_t = std::conditional_t<meta::char_like<element_t>, std::int8_t, element_t>;
        if constexpr(std::ranges::contiguous_range<Container> &&
                     std::ranges::sized_range<Container> && std::same_as<element_t, wire_t>) {
            auto data = std::ranges::data(c);
            auto len = std::ranges::size(c);
            auto off = fbb.CreateVector(data, len);
            out_offset = off.o;
            return true;
        } else {
            scalar_collector<wire_t> coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        }
    } else if constexpr(std::is_enum_v<element_t>) {
        using underlying = std::underlying_type_t<element_t>;
        scalar_collector<underlying> coll{fbb, {}, 0};
        KOTA_CODEC_TRY(body(coll));
        KOTA_CODEC_TRY(coll.finish());
        out_offset = coll.result_offset;
        return true;
    } else if constexpr(meta::str_like<element_t>) {
        string_collector coll{fbb, {}, 0};
        KOTA_CODEC_TRY(body(coll));
        KOTA_CODEC_TRY(coll.finish());
        out_offset = coll.result_offset;
        return true;
    } else if constexpr(can_inline_struct_v<element_t> && !codec::tuple_like<element_t>) {
        if constexpr(std::ranges::contiguous_range<Container> &&
                     std::ranges::sized_range<Container>) {
            auto data = std::ranges::data(c);
            auto len = std::ranges::size(c);
            auto off = fbb.CreateVectorOfStructs(data, len);
            out_offset = off.o;
            return true;
        } else {
            inline_struct_collector<element_t> coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        }
    } else if constexpr(requires {
                            typename serialize_visit<alloc_field_visitor,
                                                     element_t,
                                                     default_config<>>::wire_type;
                        }) {
        using wire_t =
            typename serialize_visit<alloc_field_visitor, element_t, default_config<>>::wire_type;
        if constexpr(std::same_as<wire_t, std::byte> || meta::bytes_like<wire_t>) {
            byte_collector coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        } else if constexpr(meta::str_like<wire_t>) {
            string_collector coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        } else if constexpr(can_inline_struct_v<wire_t> && !codec::tuple_like<wire_t>) {
            inline_struct_collector<wire_t> coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        } else {
            scalar_collector<wire_t> coll{fbb, {}, 0};
            KOTA_CODEC_TRY(body(coll));
            KOTA_CODEC_TRY(coll.finish());
            out_offset = coll.result_offset;
            return true;
        }
    } else if constexpr(meta::kind_of<element_t>() == meta::type_kind::optional ||
                        meta::kind_of<element_t>() == meta::type_kind::pointer) {
        boxed_table_collector coll{fbb, {}, 0};
        KOTA_CODEC_TRY(body(coll));
        KOTA_CODEC_TRY(coll.finish());
        out_offset = coll.result_offset;
        return true;
    } else {
        table_collector coll{fbb, {}, 0};
        KOTA_CODEC_TRY(body(coll));
        KOTA_CODEC_TRY(coll.finish());
        out_offset = coll.result_offset;
        return true;
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
    return encode_sorted_map(fbb, std::forward<Body>(body), stored_offset);
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
    KOTA_CODEC_TRY(encode_sorted_map(fbb, std::forward<Body>(body), vec_off));

    auto start = fbb.StartTable();
    fbb.AddOffset(detail::first_field, offset_t<void>(vec_off));
    table_offsets.push_back(table_offset_t(fbb.EndTable(start)));
    return true;
}

template <typename KF, typename VF>
bool map_entry_collector::visit_entry(KF&& key_fn, VF&& value_fn) {
    key_capture_visitor capture;
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
    KOTA_CODEC_TRY(encode_sorted_map(fbb, std::forward<Body>(body), vec_off));

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

template <typename Config = default_config<>, typename T>
auto to_flatbuffer(const T& value, std::optional<std::size_t> initial_capacity = std::nullopt)
    -> std::expected<std::vector<std::uint8_t>, rich_error> {
    rich_error err;
    scoped_context<rich_error> guard(err);

    encode_detail::builder_t fbb(initial_capacity.value_or(1024));
    encode_detail::root_visitor vis{fbb, {}};

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
// reference at the payload slot, so we must match that wire format.
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
