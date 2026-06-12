#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <string>
#include <string_view>

#include "kota/support/numeric.h"
#include "kota/meta/type_kind.h"
#include "kota/codec/toml/type.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/decode.h"

namespace kota::codec::toml {

namespace detail {

inline std::string_view node_type_name(const Node* node) {
    if(!node)
        return "null";
    if(node->is_boolean())
        return "boolean";
    if(node->is_integer())
        return "integer";
    if(node->is_floating_point())
        return "float";
    if(node->is_string())
        return "string";
    if(node->is_array())
        return "array";
    if(node->is_table())
        return "table";
    return "unknown";
}

template <typename T>
consteval bool is_map_like() {
    if constexpr(std::ranges::input_range<T>) {
        return meta::kind_of<T>() == meta::type_kind::map;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool root_table_v = [] {
    using U = std::remove_cvref_t<T>;
    if constexpr(meta::annotated_type<U>) {
        using inner_t = typename U::annotated_type;
        return (meta::reflectable_class<inner_t> && !is_specialization_of<std::pair, inner_t> &&
                !is_specialization_of<std::tuple, inner_t> && !std::ranges::input_range<inner_t>) ||
               is_map_like<inner_t>() || std::same_as<inner_t, Table>;
    } else {
        return (meta::reflectable_class<U> && !is_specialization_of<std::pair, U> &&
                !is_specialization_of<std::tuple, U> && !std::ranges::input_range<U>) ||
               is_map_like<U>() || std::same_as<U, Table>;
    }
}();

template <typename T>
auto select_root_node(const Table& tbl) -> const Node* {
    using U = std::remove_cvref_t<T>;

    if constexpr(is_optional_v<U>) {
        if(tbl.empty()) {
            return nullptr;
        }
        using value_t = typename U::value_type;
        if constexpr(root_table_v<value_t>) {
            return std::addressof(static_cast<const Node&>(tbl));
        } else {
            return tbl.get(boxed_root_key);
        }
    } else if constexpr(root_table_v<U>) {
        return std::addressof(static_cast<const Node&>(tbl));
    } else {
        return tbl.get(boxed_root_key);
    }
}

}  // namespace detail

struct StrReader {
    std::string_view str;
    using error_type = rich_error;

    template <typename T>
    bool visit_str(T& out) {
        out = T(str);
        return true;
    }

    template <typename T>
        requires std::is_signed_v<T>
    bool visit_int(T& out) {
        std::int64_t tmp;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), tmp);
        if(ec != std::errc{} || ptr != str.data() + str.size()) {
            return scoped_context<rich_error>::fail(
                rich_error(std::string("cannot parse '") + std::string(str) + "' as integer"));
        }
        if constexpr(sizeof(T) < sizeof(std::int64_t)) {
            if(!kota::narrow_int(tmp, out)) {
                return scoped_context<rich_error>::fail(rich_error("integer value out of range"));
            }
        } else {
            out = tmp;
        }
        return true;
    }

    template <typename T>
        requires std::is_unsigned_v<T>
    bool visit_uint(T& out) {
        std::uint64_t tmp;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), tmp);
        if(ec != std::errc{} || ptr != str.data() + str.size()) {
            return scoped_context<rich_error>::fail(rich_error(
                std::string("cannot parse '") + std::string(str) + "' as unsigned integer"));
        }
        if constexpr(sizeof(T) < sizeof(std::uint64_t)) {
            if(!kota::narrow_int(tmp, out)) {
                return scoped_context<rich_error>::fail(
                    rich_error("unsigned integer value out of range"));
            }
        } else {
            out = tmp;
        }
        return true;
    }
};

struct ValueReader {
    const Node* node;
    constexpr static bool data_driven = true;
    constexpr static bool human_readable = true;
    using error_type = rich_error;

    template <typename F>
    bool try_read(F&& fn) {
        error_type discard_err;
        scoped_context<error_type> guard(discard_err);
        ValueReader fork{node};
        return fn(fork);
    }

    bool visit_bool(bool& out) {
        if(!node || !node->is_boolean()) {
            return fail_type("boolean");
        }
        out = *node->value<bool>();
        return true;
    }

    template <typename T>
    bool visit_int(T& out) {
        if(!node || !node->is_integer()) {
            return fail_type("integer");
        }
        auto val = *node->value<std::int64_t>();
        if(!kota::narrow_int(val, out)) {
            return fail_with_location("integer value out of range");
        }
        return true;
    }

    template <typename T>
    bool visit_uint(T& out) {
        if(!node || !node->is_integer()) {
            return fail_type("integer");
        }
        auto val = *node->value<std::int64_t>();
        if(!kota::narrow_int(val, out)) {
            return fail_with_location("integer value out of range");
        }
        return true;
    }

    template <typename T>
    bool visit_float(T& out) {
        if(!node) {
            return fail_type("float");
        }
        if(node->is_floating_point()) {
            out = static_cast<T>(*node->value<double>());
            return true;
        }
        if(node->is_integer()) {
            out = static_cast<T>(*node->value<std::int64_t>());
            return true;
        }
        return fail_type("float");
    }

    template <typename T>
    bool visit_str(T& out) {
        if(!node) {
            return fail_type("string");
        }
        auto val = node->value<std::string>();
        if(!val) {
            return fail_type("string");
        }
        out = T(std::move(*val));
        return true;
    }

    template <typename T>
    bool visit_char(T& out) {
        if(!node) {
            return fail_type("string");
        }
        auto val = node->value<std::string>();
        if(!val) {
            return fail_type("string");
        }
        if(val->size() != 1) {
            return fail_with_location("expected single-character string for char");
        }
        out = static_cast<T>((*val)[0]);
        return true;
    }

    template <typename T>
    bool visit_bytes(T& out) {
        if(!node) {
            return fail_type("array");
        }
        const auto* arr = node->as_array();
        if(!arr) {
            return fail_type("array");
        }
        auto size = arr->size();
        out.clear();
        out.reserve(size);
        for(std::size_t i = 0; i < size; ++i) {
            const auto& elem = (*arr)[i];
            auto val = elem.value<std::int64_t>();
            if(!val || *val < 0 || *val > 255) {
                return fail_with_location("byte array element out of range [0, 255]");
            }
            out.push_back(static_cast<typename T::value_type>(static_cast<std::uint8_t>(*val)));
        }
        return true;
    }

    bool peek_null() {
        return node == nullptr;
    }

    bool visit_null() {
        return node == nullptr;
    }

    meta::type_kind peek_kind() {
        if(!node)
            return meta::type_kind::null;
        if(node->is_boolean())
            return meta::type_kind::boolean;
        if(node->is_integer())
            return meta::type_kind::int64;
        if(node->is_floating_point())
            return meta::type_kind::float64;
        if(node->is_string())
            return meta::type_kind::string;
        if(node->is_array())
            return meta::type_kind::array;
        if(node->is_table())
            return meta::type_kind::structure;
        return meta::type_kind::unknown;
    }

    template <typename Callback>
    bool visit_struct(Callback&& cb) {
        if(!node) {
            return fail_type("table");
        }
        const auto* tbl = node->as_table();
        if(!tbl) {
            return fail_type("table");
        }
        for(const auto& [k, v]: *tbl) {
            ValueReader sub{&v};
            KOTA_CODEC_TRY(cb(std::string_view(k), sub));
        }
        return true;
    }

    template <typename Callback>
    bool visit_seq(Callback&& cb) {
        if(!node) {
            return fail_type("array");
        }
        const auto* arr = node->as_array();
        if(!arr) {
            return fail_type("array");
        }
        for(std::size_t i = 0; i < arr->size(); ++i) {
            ValueReader sub{arr->get(i)};
            KOTA_CODEC_TRY(cb(sub));
        }
        return true;
    }

    template <typename Callback>
    bool visit_map(Callback&& cb) {
        if(!node) {
            return fail_type("table");
        }
        const auto* tbl = node->as_table();
        if(!tbl) {
            return fail_type("table");
        }
        for(const auto& [k, v]: *tbl) {
            StrReader kr{std::string_view(k)};
            ValueReader vr{&v};
            KOTA_CODEC_TRY(cb(kr, vr));
        }
        return true;
    }

    template <typename Callback>
    bool visit_tuple(Callback&& cb) {
        if(!node) {
            return fail_type("array");
        }
        const auto* arr = node->as_array();
        if(!arr) {
            return fail_type("array");
        }
        for(std::size_t i = 0; i < arr->size(); ++i) {
            ValueReader sub{arr->get(i)};
            KOTA_CODEC_TRY(cb(sub));
        }
        return true;
    }

    bool visit_skip() {
        return true;  // nothing to consume in a DOM backend
    }

private:
    bool fail_type(std::string_view expected) {
        auto got = detail::node_type_name(node);
        auto err = rich_error::invalid_type(expected, got);
        if(node) {
            auto src = node->source();
            if(src.begin.line != 0) {
                err.set_location({
                    static_cast<std::size_t>(src.begin.line),
                    static_cast<std::size_t>(src.begin.column),
                    0,
                });
            }
        }
        return scoped_context<rich_error>::fail(std::move(err));
    }

    bool fail_with_location(std::string msg) {
        rich_error err(std::move(msg));
        if(node) {
            auto src = node->source();
            if(src.begin.line != 0) {
                err.set_location({
                    static_cast<std::size_t>(src.begin.line),
                    static_cast<std::size_t>(src.begin.column),
                    0,
                });
            }
        }
        return scoped_context<rich_error>::fail(std::move(err));
    }
};

inline auto parse_table(std::string_view text) -> std::expected<Table, rich_error> {
#if TOML_EXCEPTIONS
    try {
        return ::toml::parse(text);
    } catch(const ::toml::parse_error& e) {
        rich_error err(std::string("TOML parse error: ") + e.what());
        auto src = e.source();
        if(src.begin.line != 0) {
            err.set_location({
                static_cast<std::size_t>(src.begin.line),
                static_cast<std::size_t>(src.begin.column),
                0,
            });
        }
        return std::unexpected(std::move(err));
    }
#else
    auto parsed = ::toml::parse(text);
    if(!parsed) {
        rich_error err(std::string("TOML parse error"));
        return std::unexpected(std::move(err));
    }
    return std::move(parsed).table();
#endif
}

template <typename Config = void, typename T>
auto from_toml(std::string_view toml_str, T& out) -> std::expected<void, rich_error> {
    auto table = parse_table(toml_str);
    if(!table) {
        return std::unexpected(std::move(table).error());
    }

    using V = std::remove_const_t<T>;
    const auto* root = detail::select_root_node<V>(*table);

    using Cfg = default_config<Config>;
    rich_error err;
    scoped_context<rich_error> guard(err);

    ValueReader reader{root};
    if(!decode_value<Cfg>(reader, out)) {
        return std::unexpected(std::move(err));
    }
    return {};
}

template <typename Config = void, typename T>
auto from_toml_table(const Table& tbl, T& out) -> std::expected<void, rich_error> {
    using V = std::remove_const_t<T>;
    const auto* root = detail::select_root_node<V>(tbl);

    using Cfg = default_config<Config>;
    rich_error err;
    scoped_context<rich_error> guard(err);

    ValueReader reader{root};
    if(!decode_value<Cfg>(reader, out)) {
        return std::unexpected(std::move(err));
    }
    return {};
}

}  // namespace kota::codec::toml
