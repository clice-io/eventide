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

struct table_value_writer;
struct array_value_writer;
struct table_writer;
struct array_seq_writer;
struct toml_key_writer;
struct toml_map_writer;

struct table_value_writer {
    ::toml::table& parent_table;
    std::string key;
    using error_type = rich_error;
    constexpr static bool human_readable = true;

    bool visit_bool(bool v) {
        parent_table.insert_or_assign(key, v);
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        parent_table.insert_or_assign(key, static_cast<std::int64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        auto wide = static_cast<std::uint64_t>(v);
        if(wide > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
            return scoped_context<rich_error>::fail(rich_error("uint64 exceeds int64 range"));
        }
        parent_table.insert_or_assign(key, static_cast<std::int64_t>(wide));
        return true;
    }

    template <typename T>
    bool visit_float(T v) {
        parent_table.insert_or_assign(key, static_cast<double>(v));
        return true;
    }

    template <typename T>
    bool visit_str(const T& v) {
        parent_table.insert_or_assign(key, std::string(std::string_view(v)));
        return true;
    }

    template <typename T>
    bool visit_char(T v) {
        parent_table.insert_or_assign(key, std::string(1, static_cast<char>(v)));
        return true;
    }

    template <typename T>
    bool visit_bytes(const T& v) {
        ::toml::array arr;
        auto data = reinterpret_cast<const std::uint8_t*>(std::data(v));
        auto len = std::size(v);
        for(std::size_t i = 0; i < len; ++i)
            arr.push_back(static_cast<std::int64_t>(data[i]));
        parent_table.insert_or_assign(key, std::move(arr));
        return true;
    }

    bool visit_null() {
        return true;  // skip - missing key means null in TOML
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

struct array_value_writer {
    ::toml::array& arr;
    using error_type = rich_error;
    constexpr static bool human_readable = true;

    bool visit_bool(bool v) {
        arr.push_back(v);
        return true;
    }

    template <typename T>
    bool visit_int(T v) {
        arr.push_back(static_cast<std::int64_t>(v));
        return true;
    }

    template <typename T>
    bool visit_uint(T v) {
        auto wide = static_cast<std::uint64_t>(v);
        if(wide > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
            return scoped_context<rich_error>::fail(rich_error("uint64 exceeds int64 range"));
        }
        arr.push_back(static_cast<std::int64_t>(wide));
        return true;
    }

    template <typename T>
    bool visit_float(T v) {
        arr.push_back(static_cast<double>(v));
        return true;
    }

    template <typename T>
    bool visit_str(const T& v) {
        arr.push_back(std::string(std::string_view(v)));
        return true;
    }

    template <typename T>
    bool visit_char(T v) {
        arr.push_back(std::string(1, static_cast<char>(v)));
        return true;
    }

    template <typename T>
    bool visit_bytes(const T& v) {
        ::toml::array byte_arr;
        auto data = reinterpret_cast<const std::uint8_t*>(std::data(v));
        auto len = std::size(v);
        for(std::size_t i = 0; i < len; ++i)
            byte_arr.push_back(static_cast<std::int64_t>(data[i]));
        arr.push_back(std::move(byte_arr));
        return true;
    }

    bool visit_null() {
        return scoped_context<rich_error>::fail(rich_error("TOML array does not support null"));
    }

    template <typename T, typename Body>
    inline bool visit_struct(const T&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_seq(const Container&, Body&& body);

    template <typename T, typename Body>
    inline bool visit_tuple(const T&, Body&& body);

    template <typename Container, typename Body>
    inline bool visit_map(const Container&, Body&& body);
};

struct table_writer {
    ::toml::table& tbl;
    using error_type = rich_error;

    template <typename F>
    inline bool visit_field(std::size_t /*index*/, std::string_view name, F&& writer);
};

struct array_seq_writer {
    ::toml::array& arr;

    template <typename F>
    inline bool visit_element(F&& writer);
};

struct toml_key_writer {
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

struct toml_map_writer {
    ::toml::table& tbl;

    template <typename KF, typename VF>
    inline bool visit_entry(KF&& key_fn, VF&& value_fn);
};

template <typename T, typename Body>
bool table_value_writer::visit_struct(const T&, Body&& body) {
    ::toml::table child;
    table_writer tw{child};
    KOTA_CODEC_TRY(body(tw));
    parent_table.insert_or_assign(key, std::move(child));
    return true;
}

template <typename Container, typename Body>
bool table_value_writer::visit_seq(const Container&, Body&& body) {
    ::toml::array arr;
    array_seq_writer sw{arr};
    KOTA_CODEC_TRY(body(sw));
    parent_table.insert_or_assign(key, std::move(arr));
    return true;
}

template <typename Container, typename Body>
bool table_value_writer::visit_map(const Container&, Body&& body) {
    ::toml::table child;
    toml_map_writer mw{child};
    KOTA_CODEC_TRY(body(mw));
    parent_table.insert_or_assign(key, std::move(child));
    return true;
}

template <typename T, typename Body>
bool table_value_writer::visit_tuple(const T&, Body&& body) {
    ::toml::array arr;
    array_seq_writer sw{arr};
    KOTA_CODEC_TRY(body(sw));
    parent_table.insert_or_assign(key, std::move(arr));
    return true;
}

template <typename T, typename Body>
bool array_value_writer::visit_struct(const T&, Body&& body) {
    ::toml::table child;
    table_writer tw{child};
    KOTA_CODEC_TRY(body(tw));
    arr.push_back(std::move(child));
    return true;
}

template <typename Container, typename Body>
bool array_value_writer::visit_seq(const Container&, Body&& body) {
    ::toml::array inner;
    array_seq_writer sw{inner};
    KOTA_CODEC_TRY(body(sw));
    arr.push_back(std::move(inner));
    return true;
}

template <typename T, typename Body>
bool array_value_writer::visit_tuple(const T&, Body&& body) {
    ::toml::array inner;
    array_seq_writer sw{inner};
    KOTA_CODEC_TRY(body(sw));
    arr.push_back(std::move(inner));
    return true;
}

template <typename Container, typename Body>
bool array_value_writer::visit_map(const Container&, Body&& body) {
    ::toml::table child;
    toml_map_writer mw{child};
    KOTA_CODEC_TRY(body(mw));
    arr.push_back(std::move(child));
    return true;
}

template <typename F>
bool table_writer::visit_field(std::size_t /*index*/, std::string_view name, F&& writer) {
    table_value_writer vw{tbl, std::string(name)};
    return writer(vw);
}

template <typename F>
bool array_seq_writer::visit_element(F&& writer) {
    array_value_writer vw{arr};
    return writer(vw);
}

template <typename KF, typename VF>
bool toml_map_writer::visit_entry(KF&& key_fn, VF&& value_fn) {
    std::string key;
    toml_key_writer kw{key};
    KOTA_CODEC_TRY(key_fn(kw));
    table_value_writer vw{tbl, std::move(key)};
    return value_fn(vw);
}

template <typename Config = default_config<>, typename T>
auto to_toml(const T& value) -> std::expected<::toml::table, toml::error> {
    using V = T;
    constexpr auto kind = meta::kind_of<V>();

    if constexpr(kind == meta::type_kind::optional || kind == meta::type_kind::pointer) {
        if(value) {
            return to_toml<Config>(*value);
        }
        return ::toml::table{};
    } else {
        using Cfg = default_config<Config>;
        rich_error err;
        scoped_context<rich_error> guard(err);
        ::toml::table root;

        bool ok;
        if constexpr(kind == meta::type_kind::structure) {
            table_writer tw{root};
            ok = encode_struct_fields<Cfg>(tw, value);
        } else if constexpr(kind == meta::type_kind::map) {
            toml_map_writer mw{root};
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
            table_value_writer vw{root, std::string(detail::boxed_root_key)};
            ok = encode_value<Cfg>(vw, value);
        }

        if(!ok) {
            return std::unexpected(rich_error(err.message));
        }
        return root;
    }
}

}  // namespace kota::codec::toml
