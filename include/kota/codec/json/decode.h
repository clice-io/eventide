#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "kota/codec/json/type.h"
#include "kota/codec/visit/config.h"
#include "kota/codec/visit/context.h"
#include "kota/codec/visit/decode.h"

namespace kota::codec::json {

namespace detail {

inline char32_t decode_first_codepoint(std::string_view sv) {
    if(sv.empty())
        return 0xFFFFFFFF;
    auto b0 = static_cast<unsigned char>(sv[0]);
    if(b0 < 0x80)
        return static_cast<char32_t>(b0);
    std::size_t len;
    char32_t cp;
    if((b0 & 0xE0) == 0xC0) {
        len = 2;
        cp = b0 & 0x1F;
    } else if((b0 & 0xF0) == 0xE0) {
        len = 3;
        cp = b0 & 0x0F;
    } else if((b0 & 0xF8) == 0xF0) {
        len = 4;
        cp = b0 & 0x07;
    } else {
        return 0xFFFFFFFF;
    }
    if(sv.size() < len)
        return 0xFFFFFFFF;
    for(std::size_t i = 1; i < len; ++i) {
        auto b = static_cast<unsigned char>(sv[i]);
        if((b & 0xC0) != 0x80)
            return 0xFFFFFFFF;
        cp = (cp << 6) | (b & 0x3F);
    }
    return cp;
}

}  // namespace detail

/// Tagged pointer that unifies simdjson::ondemand::document and value behind a single handle.
struct json_source {
    explicit json_source(simdjson::ondemand::value& v) :
        ptr_(reinterpret_cast<std::uintptr_t>(&v)) {}

    explicit json_source(simdjson::ondemand::document& d) :
        ptr_(reinterpret_cast<std::uintptr_t>(&d) | tag_) {}

    bool is_document() const {
        return (ptr_ & tag_) != 0;
    }

    simdjson::ondemand::document& doc() const {
        return *reinterpret_cast<simdjson::ondemand::document*>(ptr_ & ~tag_);
    }

    simdjson::ondemand::value& value() const {
        return *reinterpret_cast<simdjson::ondemand::value*>(ptr_);
    }

    template <typename F>
    decltype(auto) apply(F&& f) const {
        if(is_document())
            return f(doc());
        return f(value());
    }

private:
    std::uintptr_t ptr_;
    constexpr static std::uintptr_t tag_ = 1;
};

struct reader;

struct json_str_reader {
    std::string_view str;
    using error_type = rich_error;

    template <typename T>
    bool visit_str(T& out) {
        out = T(str);
        return true;
    }

    template <typename T>
    bool visit_int(T& out) {
        std::int64_t v = 0;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), v);
        if(ec != std::errc{} || ptr != str.data() + str.size()) {
            return scoped_context<rich_error>::fail(rich_error::invalid_type("integer", "string"));
        }
        if constexpr(sizeof(T) < sizeof(std::int64_t)) {
            if(v < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
               v > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
                return scoped_context<rich_error>::fail(rich_error("number out of range"));
            }
        }
        out = static_cast<T>(v);
        return true;
    }

    template <typename T>
    bool visit_uint(T& out) {
        std::uint64_t v = 0;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), v);
        if(ec != std::errc{} || ptr != str.data() + str.size()) {
            return scoped_context<rich_error>::fail(
                rich_error::invalid_type("unsigned integer", "string"));
        }
        if constexpr(sizeof(T) < sizeof(std::uint64_t)) {
            if(v > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
                return scoped_context<rich_error>::fail(rich_error("number out of range"));
            }
        }
        out = static_cast<T>(v);
        return true;
    }
};

struct struct_reader {
    simdjson::ondemand::object obj;
    const char* buf_base_ = nullptr;
    std::size_t buf_size_ = 0;
    using error_type = rich_error;

    template <typename Callback>
    bool visit_field(std::size_t /*index*/, std::string_view name, Callback&& cb);

    template <typename Callback>
    bool find_field(std::string_view name, Callback&& cb) {
        return visit_field(0, name, std::forward<Callback>(cb));
    }
};

struct reader {
    json_source src_;
    const char* buf_base_ = nullptr;
    std::size_t buf_size_ = 0;
    constexpr static bool data_driven = true;
    constexpr static bool human_readable = true;
    using error_type = rich_error;

    explicit reader(simdjson::ondemand::value& v) : src_(v) {}

    explicit reader(simdjson::ondemand::document& d) : src_(d) {}

    reader(simdjson::ondemand::document& d, const char* base, std::size_t size) :
        src_(d), buf_base_(base), buf_size_(size) {}

    reader(simdjson::ondemand::value& v, const char* base, std::size_t size) :
        src_(v), buf_base_(base), buf_size_(size) {}

    template <typename F>
    decltype(auto) apply(F&& f) const {
        return src_.apply(std::forward<F>(f));
    }

    bool fail_located(rich_error err) {
        if(buf_base_) {
            auto loc_result = src_.apply([](auto& s) { return s.current_location(); });
            if(!loc_result.error()) {
                const char* loc = loc_result.value_unsafe();
                if(loc >= buf_base_ && loc <= buf_base_ + buf_size_) {
                    auto offset = static_cast<std::size_t>(loc - buf_base_);
                    std::size_t line = 1, col = 1;
                    for(std::size_t i = 0; i < offset; ++i) {
                        if(buf_base_[i] == '\n') {
                            ++line;
                            col = 1;
                        } else {
                            ++col;
                        }
                    }
                    err.set_location({line, col, offset});
                }
            }
        }
        return scoped_context<rich_error>::fail(std::move(err));
    }

    template <typename F>
    bool try_read(F&& fn) {
        error_type discard_err;
        scoped_context<error_type> guard(discard_err);
        if(fn(*this))
            return true;
        if(src_.is_document())
            src_.doc().rewind();
        return false;
    }

    // --- scalar visitors ---

    bool visit_bool(bool& out) {
        auto r = src_.apply([&](auto& s) { return s.get_bool(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        out = r.value_unsafe();
        return true;
    }

    template <typename T>
    bool visit_int(T& out) {
        auto r = src_.apply([&](auto& s) { return s.get_int64(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        auto v = r.value_unsafe();
        if constexpr(sizeof(T) < sizeof(std::int64_t)) {
            if(v < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
               v > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
                return fail_located(rich_error("number out of range"));
            }
        }
        out = static_cast<T>(v);
        return true;
    }

    template <typename T>
    bool visit_uint(T& out) {
        auto r = src_.apply([&](auto& s) { return s.get_uint64(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        auto v = r.value_unsafe();
        if constexpr(sizeof(T) < sizeof(std::uint64_t)) {
            if(v > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
                return fail_located(rich_error("number out of range"));
            }
        }
        out = static_cast<T>(v);
        return true;
    }

    template <typename T>
    bool visit_float(T& out) {
        auto r = src_.apply([&](auto& s) { return s.get_double(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        out = static_cast<T>(r.value_unsafe());
        return true;
    }

    template <typename T>
    bool visit_str(T& out) {
        auto r = src_.apply([&](auto& s) { return s.get_string(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        out = T(r.value_unsafe());
        return true;
    }

    template <typename T>
    bool visit_char(T& out) {
        auto r = src_.apply([&](auto& s) { return s.get_string(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        auto sv = r.value_unsafe();
        char32_t cp = detail::decode_first_codepoint(sv);
        if(cp == 0xFFFFFFFF) {
            return fail_located(rich_error::invalid_type("single character", "multi-char string"));
        }
        std::size_t cp_len = (cp < 0x80) ? 1 : (cp < 0x800) ? 2 : (cp < 0x10000) ? 3 : 4;
        if(sv.size() != cp_len) {
            return fail_located(rich_error::invalid_type("single character", "multi-char string"));
        }
        out = static_cast<T>(cp);
        return true;
    }

    template <typename T>
    bool visit_bytes(T& out) {
        auto r = src_.apply([&](auto& s) { return s.get_array(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        out.clear();
        for(auto elem: r.value_unsafe()) {
            auto byte_r = elem.get_uint64();
            if(byte_r.error())
                return fail_located(
                    rich_error(std::string(simdjson::error_message(byte_r.error()))));
            if(byte_r.value_unsafe() > 255)
                return fail_located(rich_error("byte value out of range"));
            out.push_back(static_cast<typename T::value_type>(
                static_cast<std::uint8_t>(byte_r.value_unsafe())));
        }
        return true;
    }

    // --- null / peek ---

    bool peek_null() {
        auto r = src_.apply([&](auto& s) { return s.is_null(); });
        return !r.error() && r.value_unsafe();
    }

    bool visit_null() {
        auto r = src_.apply([&](auto& s) { return s.is_null(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        if(!r.value_unsafe())
            return fail_located(
                rich_error(std::string(simdjson::error_message(simdjson::INCORRECT_TYPE))));
        return true;
    }

    meta::type_kind peek_kind() {
        return src_.apply([&](auto& s) -> meta::type_kind {
            auto null_r = s.is_null();
            if(!null_r.error() && null_r.value_unsafe())
                return meta::type_kind::null;

            auto t = s.type();
            if(t.error())
                return meta::type_kind::unknown;
            switch(t.value_unsafe()) {
                case simdjson::ondemand::json_type::null: return meta::type_kind::null;
                case simdjson::ondemand::json_type::boolean: return meta::type_kind::boolean;
                case simdjson::ondemand::json_type::number: {
                    auto nt = s.get_number_type();
                    if(nt.error())
                        return meta::type_kind::float64;
                    switch(nt.value_unsafe()) {
                        case simdjson::ondemand::number_type::floating_point_number:
                            return meta::type_kind::float64;
                        case simdjson::ondemand::number_type::unsigned_integer:
                            return meta::type_kind::uint64;
                        case simdjson::ondemand::number_type::signed_integer:
                        default: return meta::type_kind::int64;
                    }
                }
                case simdjson::ondemand::json_type::string: return meta::type_kind::string;
                case simdjson::ondemand::json_type::array: return meta::type_kind::array;
                case simdjson::ondemand::json_type::object: return meta::type_kind::structure;
                default: return meta::type_kind::unknown;
            }
        });
    }

    bool visit_skip() {
        return true;
    }

    // --- compound visitors ---

    template <typename Callback>
    bool visit_struct(Callback&& cb) {
        auto r = src_.apply([&](auto& s) { return s.get_object(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        auto& obj = r.value_unsafe();

        if constexpr(std::is_invocable_v<Callback, std::string_view, reader&>) {
            bool ok = true;
            for(auto field_result: obj) {
                if(field_result.error()) {
                    ok = fail_located(
                        rich_error(std::string(simdjson::error_message(field_result.error()))));
                    break;
                }
                auto field = std::move(field_result).value_unsafe();
                auto key = field.unescaped_key();
                if(key.error()) {
                    ok =
                        fail_located(rich_error(std::string(simdjson::error_message(key.error()))));
                    break;
                }
                auto fv = std::move(field).value();
                reader sub{fv, buf_base_, buf_size_};
                if(!cb(key.value_unsafe(), sub)) {
                    ok = false;
                    break;
                }
            }
            obj.reset();
            return ok;
        } else {
            struct_reader sr{std::move(obj), buf_base_, buf_size_};
            return cb(sr);
        }
    }

    template <typename Callback>
    bool visit_seq(Callback&& cb) {
        auto r = src_.apply([&](auto& s) { return s.get_array(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        auto& arr = r.value_unsafe();
        bool ok = true;
        for(auto elem: arr) {
            if(elem.error()) {
                ok = fail_located(rich_error(std::string(simdjson::error_message(elem.error()))));
                break;
            }
            auto val = std::move(elem).value_unsafe();
            reader sub{val, buf_base_, buf_size_};
            if(!cb(sub)) {
                ok = false;
                break;
            }
        }
        arr.reset();
        return ok;
    }

    template <typename Callback>
    bool visit_map(Callback&& cb) {
        auto r = src_.apply([&](auto& s) { return s.get_object(); });
        if(r.error())
            return fail_located(rich_error(std::string(simdjson::error_message(r.error()))));
        auto& obj = r.value_unsafe();
        bool ok = true;
        for(auto field_result: obj) {
            if(field_result.error()) {
                ok = fail_located(
                    rich_error(std::string(simdjson::error_message(field_result.error()))));
                break;
            }
            auto field = std::move(field_result).value_unsafe();
            auto key = field.unescaped_key();
            if(key.error()) {
                ok = fail_located(rich_error(std::string(simdjson::error_message(key.error()))));
                break;
            }
            auto fv = std::move(field).value();
            json_str_reader kr{key.value_unsafe()};
            reader vr{fv, buf_base_, buf_size_};
            if(!cb(kr, vr)) {
                ok = false;
                break;
            }
        }
        obj.reset();
        return ok;
    }

    template <typename Callback>
    bool visit_tuple(Callback&& cb) {
        return visit_seq(std::forward<Callback>(cb));
    }
};

// Out-of-line definition for struct_reader

template <typename Callback>
bool struct_reader::visit_field(std::size_t /*index*/, std::string_view name, Callback&& cb) {
    simdjson::ondemand::value field_val;
    auto ec = obj.find_field_unordered(name).get(field_val);
    if(ec != success) {
        return scoped_context<rich_error>::fail(rich_error::missing_field(name));
    }
    reader sub{field_val, buf_base_, buf_size_};
    return cb(sub);
}

// Convenience functions

template <typename Config = default_config<>, typename T>
auto from_json(std::string_view json, T& out) -> std::expected<void, rich_error> {
    simdjson::padded_string padded(json);
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;

    auto ec = parser.iterate(padded).get(doc);
    if(ec != success) {
        return std::unexpected(rich_error(std::string(simdjson::error_message(ec))));
    }

    rich_error guard_error;
    scoped_context<rich_error> guard(guard_error);

    reader r{doc, padded.data(), padded.size()};
    if(!decode_value<default_config<Config>>(r, out)) {
        return std::unexpected(std::move(guard_error));
    }
    return {};
}

template <typename Config = default_config<>, typename T>
    requires std::default_initializable<T>
auto from_json(std::string_view json) -> std::expected<T, rich_error> {
    T value{};
    auto result = from_json<Config>(json, value);
    if(!result) {
        return std::unexpected(std::move(result).error());
    }
    return value;
}

}  // namespace kota::codec::json
