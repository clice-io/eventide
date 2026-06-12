#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "kota/codec/json/type.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/encode.h"

namespace kota::codec::json {

struct ValueWriter;
struct StructWriter;
struct SeqWriter;
struct KeyWriter;
struct MapWriter;

struct ValueWriter {
    StringBuilder& builder;
    using error_type = rich_error;
    constexpr static bool human_readable = true;

    bool visit_bool(bool v) {
        builder.append(v);
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        builder.append(static_cast<std::int64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        builder.append(static_cast<std::uint64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_float(T v) {
        double d = static_cast<double>(v);
        if(std::isfinite(d)) {
            builder.append(d);
        } else {
            builder.append_null();
        }
        return true;
    }

    template <typename T>
    bool visit_str(const T& v) {
        builder.escape_and_append_with_quotes(std::string_view(v));
        return true;
    }

    template <typename T>
    bool visit_char(T v) {
        char32_t cp = static_cast<char32_t>(v);
        if(cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return scoped_context<rich_error>::fail(rich_error("invalid Unicode codepoint"));
        }
        char buf[4];
        std::size_t len = 0;
        if(cp < 0x80) {
            buf[0] = static_cast<char>(cp);
            len = 1;
        } else if(cp < 0x800) {
            buf[0] = static_cast<char>(0xC0 | (cp >> 6));
            buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
            len = 2;
        } else if(cp < 0x10000) {
            buf[0] = static_cast<char>(0xE0 | (cp >> 12));
            buf[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            buf[2] = static_cast<char>(0x80 | (cp & 0x3F));
            len = 3;
        } else {
            buf[0] = static_cast<char>(0xF0 | (cp >> 18));
            buf[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            buf[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            buf[3] = static_cast<char>(0x80 | (cp & 0x3F));
            len = 4;
        }
        builder.escape_and_append_with_quotes(std::string_view(buf, len));
        return true;
    }

    template <typename T>
    bool visit_bytes(const T& v) {
        auto data = reinterpret_cast<const std::uint8_t*>(std::data(v));
        auto len = std::size(v);
        builder.start_array();
        for(std::size_t i = 0; i < len; ++i) {
            if(i > 0)
                builder.append_comma();
            builder.append(static_cast<std::uint64_t>(data[i]));
        }
        builder.end_array();
        return true;
    }

    bool visit_null() {
        builder.append_null();
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

struct StructWriter {
    StringBuilder& builder;
    using error_type = rich_error;
    bool first = true;

    template <typename F>
    inline bool visit_field(std::size_t /*index*/, std::string_view name, F&& writer);
};

struct SeqWriter {
    StringBuilder& builder;
    bool first = true;

    template <typename F>
    inline bool visit_element(F&& writer);
};

struct KeyWriter {
    StringBuilder& builder;
    using error_type = rich_error;

    template <typename T>
    bool visit_str(const T& v) {
        builder.escape_and_append_with_quotes(std::string_view(v));
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        auto s = std::to_string(static_cast<std::int64_t>(v));
        builder.escape_and_append_with_quotes(std::string_view(s));
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        auto s = std::to_string(static_cast<std::uint64_t>(v));
        builder.escape_and_append_with_quotes(std::string_view(s));
        return true;
    }
};

struct MapWriter {
    StringBuilder& builder;
    bool first = true;

    template <typename KF, typename VF>
    inline bool visit_entry(KF&& key_fn, VF&& value_fn);
};

template <typename T, typename Body>
bool ValueWriter::visit_struct(const T&, Body&& body) {
    builder.start_object();
    StructWriter sw{builder};
    KOTA_CODEC_TRY(body(sw));
    builder.end_object();
    return true;
}

template <typename Container, typename Body>
bool ValueWriter::visit_seq(const Container&, Body&& body) {
    builder.start_array();
    SeqWriter sw{builder};
    KOTA_CODEC_TRY(body(sw));
    builder.end_array();
    return true;
}

template <typename Container, typename Body>
bool ValueWriter::visit_map(const Container&, Body&& body) {
    builder.start_object();
    MapWriter mw{builder};
    KOTA_CODEC_TRY(body(mw));
    builder.end_object();
    return true;
}

template <typename T, typename Body>
bool ValueWriter::visit_tuple(const T&, Body&& body) {
    builder.start_array();
    SeqWriter sw{builder};
    KOTA_CODEC_TRY(body(sw));
    builder.end_array();
    return true;
}

template <typename F>
bool StructWriter::visit_field(std::size_t /*index*/, std::string_view name, F&& writer) {
    if(!first)
        builder.append_comma();
    first = false;
    builder.escape_and_append_with_quotes(name);
    builder.append_colon();
    ValueWriter vw{builder};
    return writer(vw);
}

template <typename F>
bool SeqWriter::visit_element(F&& writer) {
    if(!first)
        builder.append_comma();
    first = false;
    ValueWriter vw{builder};
    return writer(vw);
}

template <typename KF, typename VF>
bool MapWriter::visit_entry(KF&& key_fn, VF&& value_fn) {
    if(!first)
        builder.append_comma();
    first = false;
    KeyWriter kw{builder};
    KOTA_CODEC_TRY(key_fn(kw));
    builder.append_colon();
    ValueWriter vw{builder};
    return value_fn(vw);
}

template <typename Config = void, typename T>
auto to_json(const T& value, std::optional<std::size_t> initial_capacity = std::nullopt)
    -> std::expected<std::string, json::error> {
    rich_error err;
    scoped_context<rich_error> guard(err);
    StringBuilder builder(initial_capacity.value_or(StringBuilder::DEFAULT_INITIAL_CAPACITY));
    ValueWriter vis{builder};
    if(!encode_value<default_config<Config>>(vis, value)) {
        return std::unexpected(std::move(err));
    }
    std::string_view sv;
    auto ec = builder.view().get(sv);
    if(ec != success) {
        return std::unexpected(rich_error("write failed"));
    }
    return std::string(sv);
}

}  // namespace kota::codec::json
