#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

#include "kota/meta/type_kind.h"
#include "kota/codec/toml/type.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/encode.h"

namespace kota::codec::toml {

template <typename Sink>
struct ValueWriter;
struct TableSink;
struct ArraySink;
struct TableWriter;
struct ArraySeqWriter;
struct KeyWriter;
struct MapWriter;

using TableValueWriter = ValueWriter<TableSink>;
using ArrayValueWriter = ValueWriter<ArraySink>;

struct TableSink {
    Table& tbl;
    std::string key;

    template <typename V>
    bool emit(V&& v) {
        tbl.insert_or_assign(key, std::forward<V>(v));
        return true;
    }

    bool emit_null() {
        return true;
    }
};

struct ArraySink {
    Array& arr;

    template <typename V>
    bool emit(V&& v) {
        arr.push_back(std::forward<V>(v));
        return true;
    }

    bool emit_null() {
        return scoped_context<rich_error>::fail(rich_error("TOML array does not support null"));
    }
};

template <typename Sink>
struct ValueWriter {
    Sink sink;
    using error_type = rich_error;
    constexpr static bool human_readable = true;

    bool visit_null() {
        return sink.emit_null();
    }

    bool visit_bool(bool v) {
        return sink.emit(v);
    }

    template <typename T>
    bool visit_int(T v) {
        return sink.emit(static_cast<std::int64_t>(v));
    }

    template <typename T>
    bool visit_uint(T v) {
        auto wide = static_cast<std::uint64_t>(v);
        if(wide > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
            return scoped_context<rich_error>::fail(rich_error("uint64 exceeds int64 range"));
        }
        return sink.emit(static_cast<std::int64_t>(wide));
    }

    template <typename T>
    bool visit_float(T v) {
        return sink.emit(static_cast<double>(v));
    }

    template <typename T>
    bool visit_str(const T& v) {
        return sink.emit(std::string(std::string_view(v)));
    }

    template <typename T>
    bool visit_char(T v) {
        return sink.emit(std::string(1, static_cast<char>(v)));
    }

    template <typename T>
    bool visit_bytes(const T& v) {
        Array byte_arr;
        auto data = reinterpret_cast<const std::uint8_t*>(std::data(v));
        auto len = std::size(v);
        for(std::size_t i = 0; i < len; ++i)
            byte_arr.push_back(static_cast<std::int64_t>(data[i]));
        return sink.emit(std::move(byte_arr));
    }

    template <typename T, typename Body>
    inline bool visit_struct(const T&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_seq(const Container&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_map(const Container&, Body&& body);

    template <typename T, typename Body>
    inline bool visit_tuple(const T&, Body&& body);
};

struct TableWriter {
    Table& tbl;
    using error_type = rich_error;

    template <typename F>
    inline bool visit_field(std::size_t /*index*/, std::string_view name, F&& writer);
};

struct ArraySeqWriter {
    Array& arr;

    template <typename F>
    inline bool visit_element(F&& writer);
};

struct KeyWriter {
    std::string& output;
    using error_type = rich_error;

    template <typename T>
    bool visit_str(const T& v) {
        output = std::string(std::string_view(v));
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        output = std::to_string(static_cast<std::int64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        output = std::to_string(static_cast<std::uint64_t>(v));
        return true;
    }
};

struct MapWriter {
    Table& tbl;

    template <typename KF, typename VF>
    inline bool visit_entry(KF&& key_fn, VF&& value_fn);
};

template <typename Sink>
template <typename T, typename Body>
bool ValueWriter<Sink>::visit_struct(const T&, Body&& body) {
    Table child;
    TableWriter tw{child};
    KOTA_CODEC_TRY(body(tw));
    return sink.emit(std::move(child));
}

template <typename Sink>
template <typename Container, typename Body>
bool ValueWriter<Sink>::visit_seq(const Container&, Body&& body) {
    Array arr;
    ArraySeqWriter sw{arr};
    KOTA_CODEC_TRY(body(sw));
    return sink.emit(std::move(arr));
}

template <typename Sink>
template <typename Container, typename Body>
bool ValueWriter<Sink>::visit_map(const Container&, Body&& body) {
    Table child;
    MapWriter mw{child};
    KOTA_CODEC_TRY(body(mw));
    return sink.emit(std::move(child));
}

template <typename Sink>
template <typename T, typename Body>
bool ValueWriter<Sink>::visit_tuple(const T&, Body&& body) {
    Array arr;
    ArraySeqWriter sw{arr};
    KOTA_CODEC_TRY(body(sw));
    return sink.emit(std::move(arr));
}

template <typename F>
bool TableWriter::visit_field(std::size_t /*index*/, std::string_view name, F&& writer) {
    TableValueWriter vw{
        {tbl, std::string(name)}
    };
    return writer(vw);
}

template <typename F>
bool ArraySeqWriter::visit_element(F&& writer) {
    ArrayValueWriter vw{{arr}};
    return writer(vw);
}

template <typename KF, typename VF>
bool MapWriter::visit_entry(KF&& key_fn, VF&& value_fn) {
    std::string key;
    KeyWriter kw{key};
    KOTA_CODEC_TRY(key_fn(kw));
    TableValueWriter vw{
        {tbl, std::move(key)}
    };
    return value_fn(vw);
}

template <typename Config = void, typename T>
auto to_toml(const T& value) -> std::expected<Table, toml::error> {
    using V = T;
    constexpr auto kind = meta::kind_of<V>();

    if constexpr(kind == meta::type_kind::optional || kind == meta::type_kind::pointer) {
        if(value) {
            return to_toml<Config>(*value);
        }
        return Table{};
    } else {
        using Cfg = default_config<Config>;
        rich_error err;
        scoped_context<rich_error> guard(err);
        Table root;

        bool ok;
        if constexpr(kind == meta::type_kind::structure) {
            TableWriter tw{root};
            ok = encode_struct_fields<Cfg>(tw, value);
        } else if constexpr(kind == meta::type_kind::map) {
            MapWriter mw{root};
            ok = [&]() -> bool {
                for(const auto& [k, v]: value) {
                    bool entry_ok =
                        mw.visit_entry([&](auto& kv) -> bool { return encode_value<Cfg>(kv, k); },
                                       [&](auto& vv) -> bool { return encode_value<Cfg>(vv, v); });
                    if(!entry_ok)
                        return false;
                }
                return true;
            }();
        } else {
            TableValueWriter vw{
                {root, std::string(detail::boxed_root_key)}
            };
            ok = encode_value<Cfg>(vw, value);
        }

        if(!ok) {
            return std::unexpected(std::move(err));
        }
        return root;
    }
}

}  // namespace kota::codec::toml
