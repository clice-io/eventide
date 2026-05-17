#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <string>
#include <string_view>

#include "kota/codec/dyn/document.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/encode.h"

namespace kota::codec::dyn {

struct value_writer;
struct struct_writer;
struct seq_writer;
struct key_writer;
struct map_writer;

struct value_writer {
    dyn::Value& output;
    using error_type = rich_error;
    constexpr static bool human_readable = true;

    bool visit_bool(bool v) {
        output = dyn::Value(v);
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        output = dyn::Value(static_cast<std::int64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        output = dyn::Value(static_cast<std::uint64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_float(T v) {
        double d = static_cast<double>(v);
        if(std::isfinite(d)) {
            output = dyn::Value(d);
        } else {
            output = dyn::Value(nullptr);
        }
        return true;
    }

    template <typename T>
    bool visit_str(const T& v) {
        output = dyn::Value(std::string(std::string_view(v)));
        return true;
    }

    template <typename T>
    bool visit_char(T v) {
        output = dyn::Value(std::string(1, static_cast<char>(v)));
        return true;
    }

    template <typename T>
    bool visit_bytes(const T& v) {
        auto data = reinterpret_cast<const std::uint8_t*>(std::data(v));
        auto len = std::size(v);
        dyn::Array arr;
        for(std::size_t i = 0; i < len; ++i) {
            arr.push_back(dyn::Value(static_cast<std::uint64_t>(data[i])));
        }
        output = dyn::Value(std::move(arr));
        return true;
    }

    bool visit_null() {
        output = dyn::Value(nullptr);
        return true;
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

struct struct_writer {
    dyn::Object& obj;
    using error_type = rich_error;

    template <typename F>
    inline bool visit_field(std::size_t /*index*/, std::string_view name, F&& writer);
};

struct seq_writer {
    dyn::Array& arr;

    template <typename F>
    inline bool visit_element(F&& writer);
};

struct key_writer {
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

struct map_writer {
    dyn::Object& obj;

    template <typename KF, typename VF>
    inline bool visit_entry(KF&& key_fn, VF&& value_fn);
};

template <typename T, typename Body>
bool value_writer::visit_struct(const T&, Body&& body) {
    dyn::Object obj;
    struct_writer sw{obj};
    KOTA_CODEC_TRY(body(sw));
    output = dyn::Value(std::move(obj));
    return true;
}

template <typename Container, typename Body>
bool value_writer::visit_seq(const Container&, Body&& body) {
    dyn::Array arr;
    seq_writer sw{arr};
    KOTA_CODEC_TRY(body(sw));
    output = dyn::Value(std::move(arr));
    return true;
}

template <typename Container, typename Body>
bool value_writer::visit_map(const Container&, Body&& body) {
    dyn::Object obj;
    map_writer mw{obj};
    KOTA_CODEC_TRY(body(mw));
    output = dyn::Value(std::move(obj));
    return true;
}

template <typename T, typename Body>
bool value_writer::visit_tuple(const T&, Body&& body) {
    dyn::Array arr;
    seq_writer sw{arr};
    KOTA_CODEC_TRY(body(sw));
    output = dyn::Value(std::move(arr));
    return true;
}

template <typename F>
bool struct_writer::visit_field(std::size_t /*index*/, std::string_view name, F&& writer) {
    dyn::Value field_val;
    value_writer vw{field_val};
    KOTA_CODEC_TRY(writer(vw));
    obj.insert(std::string(name), std::move(field_val));
    return true;
}

template <typename F>
bool seq_writer::visit_element(F&& writer) {
    dyn::Value elem_val;
    value_writer vw{elem_val};
    KOTA_CODEC_TRY(writer(vw));
    arr.push_back(std::move(elem_val));
    return true;
}

template <typename KF, typename VF>
bool map_writer::visit_entry(KF&& key_fn, VF&& value_fn) {
    std::string key;
    key_writer kw{key};
    KOTA_CODEC_TRY(key_fn(kw));
    dyn::Value val;
    value_writer vw{val};
    KOTA_CODEC_TRY(value_fn(vw));
    obj.insert(std::move(key), std::move(val));
    return true;
}

template <typename Config = default_config<>, typename T>
auto to_content(const T& value) -> std::expected<dyn::Value, rich_error> {
    rich_error err;
    scoped_context<rich_error> guard(err);
    dyn::Value result;
    value_writer vis{result};
    if(!encode_value<default_config<Config>>(vis, value)) {
        return std::unexpected(std::move(err));
    }
    return result;
}

}  // namespace kota::codec::dyn

namespace kota::codec {

template <typename Vis, typename Config>
struct serialize_visit<Vis, dyn::Value, Config> {
    static bool visit(Vis& vis, const dyn::Value& value) {
        return std::visit(
            [&](const auto& stored) -> bool { return encode_value<Config>(vis, stored); },
            value.variant());
    }
};

template <typename Config>
struct serialize_visit<dyn::value_writer, dyn::Value, Config> {
    static bool visit(dyn::value_writer& vis, const dyn::Value& value) {
        vis.output = value;
        return true;
    }
};

template <typename Config>
struct serialize_visit<dyn::value_writer, dyn::Array, Config> {
    static bool visit(dyn::value_writer& vis, const dyn::Array& value) {
        vis.output = dyn::Value(value);
        return true;
    }
};

template <typename Config>
struct serialize_visit<dyn::value_writer, dyn::Object, Config> {
    static bool visit(dyn::value_writer& vis, const dyn::Object& value) {
        vis.output = dyn::Value(value);
        return true;
    }
};

}  // namespace kota::codec
