#pragma once

#include <cstdlib>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "eventide/error.h"
#include "reflection/struct.h"
#include "reflection/traits.h"
#include "serde/attrs.h"
#include "serde/enum.h"

namespace eventide::serde {

template <typename T>
using result = eventide::result<T>;

template <class T>
struct codec {
    static constexpr bool enabled = false;
};

template <class T>
constexpr inline bool has_codec_v = codec<T>::enabled;

namespace detail {

template <class...>
constexpr inline bool always_false_v = false;

template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <class T>
constexpr inline bool is_string_v = std::is_same_v<remove_cvref_t<T>, std::string> ||
                                    std::is_same_v<remove_cvref_t<T>, std::string_view>;

template <class T>
struct is_optional : std::false_type {};
template <class T>
struct is_optional<std::optional<T>> : std::true_type {};
template <class T>
constexpr inline bool is_optional_v = is_optional<remove_cvref_t<T>>::value;

template <class T>
struct is_vector : std::false_type {};
template <class T, class A>
struct is_vector<std::vector<T, A>> : std::true_type {};
template <class T>
constexpr inline bool is_vector_v = is_vector<remove_cvref_t<T>>::value;

template <class T>
struct is_map : std::false_type {};
template <class K, class V, class C, class A>
struct is_map<std::map<K, V, C, A>> : std::true_type {};
template <class T>
constexpr inline bool is_map_v = is_map<remove_cvref_t<T>>::value;

template <class T>
struct is_variant : std::false_type {};
template <class... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};
template <class T>
constexpr inline bool is_variant_v = is_variant<remove_cvref_t<T>>::value;

template <class T>
struct is_tuple : std::false_type {};
template <class... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};
template <class T>
constexpr inline bool is_tuple_v = is_tuple<remove_cvref_t<T>>::value;

template <class T>
constexpr inline bool is_attr_v = requires { T::kind; };

template <class T>
constexpr inline AttrKind attr_kind_v = T::kind;

template <class T>
using attr_value_t = typename T::value_type;

template <class T>
decltype(auto) attr_value(T& v) {
    if constexpr (requires { v.value; }) {
        return (v.value);
    } else {
        return static_cast<attr_value_t<T>&>(v);
    }
}

template <class T>
decltype(auto) attr_value(const T& v) {
    if constexpr (requires { v.value; }) {
        return (v.value);
    } else {
        return static_cast<const attr_value_t<T>&>(v);
    }
}

template <class T, class V>
void assign_attr(T& target, V&& value) {
    if constexpr (requires { target.value = std::forward<V>(value); }) {
        target.value = std::forward<V>(value);
    } else {
        static_cast<attr_value_t<T>&>(target) = std::forward<V>(value);
    }
}

template <class T>
constexpr inline bool is_reflectable_v =
    refl::traits::aggregate_type<remove_cvref_t<T>>;

template <class T>
constexpr inline bool is_transparent_struct_v = [] {
    if constexpr (!is_reflectable_v<T>) {
        return false;
    } else {
        using refs_t = remove_cvref_t<decltype(refl::member_refs(std::declval<T&>()))>;
        if constexpr (std::tuple_size_v<refs_t> != 1) {
            return false;
        } else {
            using field_t = remove_cvref_t<decltype(std::get<0>(std::declval<refs_t&>()))>;
            return is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::transparent;
        }
    }
}();

template <class K>
std::string key_to_string(const K& key) {
    if constexpr (std::is_same_v<remove_cvref_t<K>, std::string>) {
        return key;
    } else if constexpr (std::is_same_v<remove_cvref_t<K>, std::string_view>) {
        return std::string(key);
    } else if constexpr (std::is_integral_v<remove_cvref_t<K>>) {
        return std::to_string(key);
    } else {
        static_assert(always_false_v<K>, "Map key type must be string-like or integral.");
    }
}

template <class K>
bool parse_key(std::string_view text, K& out) {
    if constexpr (std::is_same_v<remove_cvref_t<K>, std::string>) {
        out = std::string(text);
        return true;
    } else if constexpr (std::is_same_v<remove_cvref_t<K>, std::string_view>) {
        out = text;
        return true;
    } else if constexpr (std::is_integral_v<remove_cvref_t<K>>) {
        std::string tmp(text);
        if(tmp.empty()) {
            return false;
        }
        char* end = nullptr;
        if constexpr (std::is_signed_v<remove_cvref_t<K>>) {
            long long v = std::strtoll(tmp.c_str(), &end, 10);
            if(end == tmp.c_str() || *end != '\0') {
                return false;
            }
            out = static_cast<remove_cvref_t<K>>(v);
        } else {
            unsigned long long v = std::strtoull(tmp.c_str(), &end, 10);
            if(end == tmp.c_str() || *end != '\0') {
                return false;
            }
            out = static_cast<remove_cvref_t<K>>(v);
        }
        return true;
    } else {
        return false;
    }
}

}  // namespace detail

template <class Reader>
using value_type_t = typename Reader::value_type;

template <class Reader>
using object_type_t = typename Reader::object_type;

template <class Reader>
using array_type_t = typename Reader::array_type;

namespace detail {

template <class Writer, class T>
void write_value(Writer& w, const T& value);

template <class Writer, class T>
void write_object_fields(Writer& w, const T& object);

template <class Writer, class T>
void write_object(Writer& w, const T& object) {
    w.begin_object();
    write_object_fields(w, object);
    w.end_object();
}

template <class Writer, class T>
void write_object_fields(Writer& w, const T& object) {
    auto refs = refl::member_refs(object);
    using refs_t = remove_cvref_t<decltype(refs)>;
    constexpr std::size_t N = std::tuple_size_v<refs_t>;
    const auto& names = refl::reflection<remove_cvref_t<T>>::member_names;
    auto write_one = [&](auto index_const) {
        constexpr std::size_t I = decltype(index_const)::value;
        auto& field = std::get<I>(refs);
        using field_t = remove_cvref_t<decltype(field)>;

        if constexpr (is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::skip) {
            return;
        }

        auto key = names[I];
        if constexpr (is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::rename) {
            key = field_t::name;
        }

        if constexpr (is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::skip_if) {
            using pred_t = typename field_t::pred_type;
            if (pred_t{}(attr_value(field))) {
                return;
            }
        }

        if constexpr (is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::flatten) {
            write_object_fields(w, attr_value(field));
            return;
        }

        w.key(key);

        if constexpr (is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::literal) {
            w.value(std::string_view(field_t::value));
        } else if constexpr (is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::with) {
            using codec_t = typename field_t::codec_type;
            codec_t::write(attr_value(field), w);
        } else {
            if constexpr (is_attr_v<field_t>) {
                write_value(w, attr_value(field));
            } else {
                write_value(w, field);
            }
        }
    };

    // Fold expression over index sequence.
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (write_one(std::integral_constant<std::size_t, Is>{}), ...);
    }(std::make_index_sequence<N>{});
}

template <class Writer, class T>
void write_value(Writer& w, const T& value) {
    if constexpr (is_attr_v<T>) {
        if constexpr (attr_kind_v<T> == AttrKind::skip) {
            return;
        } else if constexpr (attr_kind_v<T> == AttrKind::literal) {
            w.value(std::string_view(T::value));
        } else if constexpr (attr_kind_v<T> == AttrKind::with) {
            using codec_t = typename T::codec_type;
            codec_t::write(attr_value(value), w);
        } else {
            write_value(w, attr_value(value));
        }
    } else if constexpr (has_codec_v<T>) {
        codec<T>::write(value, w);
    } else if constexpr (detail::is_optional_v<T>) {
        if (value) {
            write_value(w, *value);
        } else {
            w.null();
        }
    } else if constexpr (detail::is_vector_v<T>) {
        w.begin_array();
        for (const auto& item : value) {
            write_value(w, item);
        }
        w.end_array();
    } else if constexpr (detail::is_map_v<T>) {
        w.begin_object();
        for (const auto& [k, v] : value) {
            auto key = detail::key_to_string(k);
            w.key(key);
            write_value(w, v);
        }
        w.end_object();
    } else if constexpr (detail::is_variant_v<T>) {
        std::visit([&](const auto& item) { write_value(w, item); }, value);
    } else if constexpr (detail::is_tuple_v<T>) {
        w.begin_array();
        std::apply([&](const auto&... items) { (write_value(w, items), ...); }, value);
        w.end_array();
    } else if constexpr (refl::serde::has_enum_traits<T>) {
        if constexpr (refl::serde::enum_traits<T>::encoding ==
                      refl::serde::enum_encoding::integer) {
            w.value(static_cast<std::underlying_type_t<T>>(value));
        } else {
            w.value(refl::serde::enum_to_string(value));
        }
    } else if constexpr (std::is_enum_v<T>) {
        w.value(static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (detail::is_string_v<T>) {
        w.value(std::string_view(value));
    } else if constexpr (std::is_same_v<detail::remove_cvref_t<T>, bool>) {
        w.value(value);
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            w.value(static_cast<std::int64_t>(value));
        } else {
            w.value(static_cast<std::uint64_t>(value));
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        w.value(static_cast<double>(value));
    } else if constexpr (detail::is_transparent_struct_v<T>) {
        auto refs = refl::member_refs(value);
        auto& field = std::get<0>(refs);
        if constexpr (detail::is_attr_v<remove_cvref_t<decltype(field)>>) {
            write_value(w, detail::attr_value(field));
        } else {
            write_value(w, field);
        }
    } else if constexpr (detail::is_reflectable_v<T>) {
        write_object(w, value);
    } else {
        static_assert(always_false_v<T>, "Unsupported type for serialization.");
    }
}

template <class Reader, class T>
result<T> read_value(Reader& r, value_type_t<Reader> v);

template <class Reader, class T>
result<void> read_object_into(Reader& r, object_type_t<Reader> obj, T& out);

template <class Reader, class T>
result<T> read_object(Reader& r, value_type_t<Reader> v) {
    auto obj = r.as_object(v);
    if (!obj) {
        return std::unexpected(obj.error());
    }
    T out{};
    auto err = read_object_into(r, *obj, out);
    if (!err) {
        return std::unexpected(err.error());
    }
    return out;
}

template <class Reader, class T>
result<void> read_object_into(Reader& r, object_type_t<Reader> obj, T& out) {
    auto refs = refl::member_refs(out);
    using refs_t = remove_cvref_t<decltype(refs)>;
    constexpr std::size_t N = std::tuple_size_v<refs_t>;
    const auto& names = refl::reflection<remove_cvref_t<T>>::member_names;

    auto handle_missing = [&](auto& field) -> result<void> {
        using field_t = remove_cvref_t<decltype(field)>;
        if constexpr (detail::is_optional_v<field_t>) {
            return {};
        } else if constexpr (detail::is_attr_v<field_t> &&
                             attr_kind_v<field_t> == AttrKind::default_value) {
            return {};
        } else if constexpr (detail::is_attr_v<field_t> &&
                             attr_kind_v<field_t> == AttrKind::literal) {
            return std::unexpected(eventide::error::invalid_argument);
        } else {
            return std::unexpected(eventide::error::invalid_argument);
        }
    };

    auto read_one = [&](auto index_const) -> result<void> {
        constexpr std::size_t I = decltype(index_const)::value;
        auto& field = std::get<I>(refs);
        using field_t = remove_cvref_t<decltype(field)>;

        if constexpr (detail::is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::flatten) {
            return read_object_into(r, obj, detail::attr_value(field));
        }

        std::string_view key = names[I];
        if constexpr (detail::is_attr_v<field_t> && attr_kind_v<field_t> == AttrKind::rename) {
            key = field_t::name;
        }

        auto try_read_key = [&](std::string_view k) -> result<bool> {
            auto found = r.find(obj, k);
            if (found) {
                if constexpr (detail::is_attr_v<field_t> &&
                              attr_kind_v<field_t> == AttrKind::skip) {
                    return true;
                }

                if constexpr (detail::is_attr_v<field_t> &&
                              attr_kind_v<field_t> == AttrKind::literal) {
                    auto s = r.as_string(*found);
                    if (!s || *s != std::string_view(field_t::value)) {
                        return std::unexpected(eventide::error::invalid_argument);
                    }
                    return true;
                }

                if constexpr (detail::is_attr_v<field_t> &&
                              attr_kind_v<field_t> == AttrKind::with) {
                    using codec_t = typename field_t::codec_type;
                    auto parsed = codec_t::read(r, *found);
                    if (!parsed) {
                        return std::unexpected(parsed.error());
                    }
                    detail::assign_attr(field, std::move(*parsed));
                    return true;
                }

                using value_t =
                    std::conditional_t<detail::is_attr_v<field_t>,
                                       detail::attr_value_t<field_t>, field_t>;
                auto parsed = read_value<Reader, value_t>(r, *found);
                if (!parsed) {
                    return std::unexpected(parsed.error());
                }
                if constexpr (detail::is_attr_v<field_t>) {
                    detail::assign_attr(field, std::move(*parsed));
                } else {
                    field = std::move(*parsed);
                }
                return true;
            }
            if (found.error() == eventide::error::no_data_available) {
                return false;
            }
            return std::unexpected(found.error());
        };

        if constexpr (detail::is_attr_v<field_t> &&
                      attr_kind_v<field_t> == AttrKind::alias) {
            for (auto name : field_t::names) {
                auto got = try_read_key(name);
                if (!got) {
                    return std::unexpected(got.error());
                }
                if (*got) {
                    return {};
                }
            }
        }

        auto got = try_read_key(key);
        if (!got) {
            return std::unexpected(got.error());
        }
        if (*got) {
            return {};
        }
        return handle_missing(field);
    };

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> result<void> {
        result<void> res{};
        ((res = read_one(std::integral_constant<std::size_t, Is>{}),
          res ? void() : void()), ...);
        return res;
    }(std::make_index_sequence<N>{});
}

template <class Reader, class T>
result<T> read_value(Reader& r, value_type_t<Reader> v) {
    if constexpr (detail::is_attr_v<T>) {
        using value_t = detail::attr_value_t<T>;
        auto parsed = read_value<Reader, value_t>(r, v);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        T out{};
        detail::assign_attr(out, std::move(*parsed));
        return out;
    } else if constexpr (has_codec_v<T>) {
        return codec<T>::read(r, v);
    } else if constexpr (detail::is_optional_v<T>) {
        if (r.is_null(v)) {
            return std::optional<typename T::value_type>{};
        }
        auto inner = read_value<Reader, typename T::value_type>(r, v);
        if (!inner) {
            return std::unexpected(inner.error());
        }
        return std::optional<typename T::value_type>(std::move(*inner));
    } else if constexpr (detail::is_vector_v<T>) {
        auto arr = r.as_array(v);
        if (!arr) {
            return std::unexpected(arr.error());
        }
        T out{};
        auto err = r.for_each_element(*arr, [&](value_type_t<Reader> item) -> result<void> {
            auto parsed = read_value<Reader, typename T::value_type>(r, item);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            out.push_back(std::move(*parsed));
            return {};
        });
        if (!err) {
            return std::unexpected(err.error());
        }
        return out;
    } else if constexpr (detail::is_map_v<T>) {
        auto obj = r.as_object(v);
        if (!obj) {
            return std::unexpected(obj.error());
        }
        T out{};
        auto err = r.for_each_member(*obj, [&](std::string_view k, value_type_t<Reader> item)
                                         -> result<void> {
            typename T::key_type key{};
            if (!detail::parse_key(k, key)) {
                return std::unexpected(eventide::error::invalid_argument);
            }
            auto parsed = read_value<Reader, typename T::mapped_type>(r, item);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            out.emplace(std::move(key), std::move(*parsed));
            return {};
        });
        if (!err) {
            return std::unexpected(err.error());
        }
        return out;
    } else if constexpr (detail::is_variant_v<T>) {
        result<T> res = std::unexpected(eventide::error::invalid_argument);
        bool matched = false;
        auto try_one = [&](auto tag) {
            using alt_t = std::variant_alternative_t<tag, T>;
            if (matched) {
                return;
            }
            auto parsed = read_value<Reader, alt_t>(r, v);
            if (parsed) {
                res = T{std::in_place_type<alt_t>, std::move(*parsed)};
                matched = true;
            }
        };
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (try_one(std::integral_constant<std::size_t, Is>{}), ...);
        }(std::make_index_sequence<std::variant_size_v<T>>{});
        return res;
    } else if constexpr (detail::is_tuple_v<T>) {
        auto arr = r.as_array(v);
        if (!arr) {
            return std::unexpected(arr.error());
        }
        T out{};
        std::size_t index = 0;
        auto err = r.for_each_element(*arr, [&](value_type_t<Reader> item) -> result<void> {
            bool assigned = false;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((index == Is
                      ? (void)([&] {
                            using elem_t = std::tuple_element_t<Is, T>;
                            auto parsed = read_value<Reader, elem_t>(r, item);
                            if (parsed) {
                                std::get<Is>(out) = std::move(*parsed);
                                assigned = true;
                            }
                        }())
                      : void()),
                 ...);
            }(std::make_index_sequence<std::tuple_size_v<T>>{});
            ++index;
            if (!assigned) {
                return std::unexpected(eventide::error::invalid_argument);
            }
            return {};
        });
        if (!err) {
            return std::unexpected(err.error());
        }
        return out;
    } else if constexpr (refl::serde::has_enum_traits<T>) {
        if constexpr (refl::serde::enum_traits<T>::encoding ==
                      refl::serde::enum_encoding::integer) {
            auto val = r.as_i64(v);
            if (!val) {
                return std::unexpected(val.error());
            }
            return static_cast<T>(*val);
        } else {
            auto s = r.as_string(v);
            if (!s) {
                return std::unexpected(s.error());
            }
            T out{};
            if (!refl::serde::enum_from_string(*s, out)) {
                return std::unexpected(eventide::error::invalid_argument);
            }
            return out;
        }
    } else if constexpr (std::is_enum_v<T>) {
        auto val = r.as_i64(v);
        if (!val) {
            return std::unexpected(val.error());
        }
        return static_cast<T>(*val);
    } else if constexpr (detail::is_string_v<T>) {
        auto s = r.as_string(v);
        if (!s) {
            return std::unexpected(s.error());
        }
        if constexpr (std::is_same_v<detail::remove_cvref_t<T>, std::string>) {
            return std::string(*s);
        } else {
            return *s;
        }
    } else if constexpr (std::is_same_v<detail::remove_cvref_t<T>, bool>) {
        return r.as_bool(v);
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            auto val = r.as_i64(v);
            if (!val) {
                return std::unexpected(val.error());
            }
            return static_cast<T>(*val);
        } else {
            auto val = r.as_u64(v);
            if (!val) {
                return std::unexpected(val.error());
            }
            return static_cast<T>(*val);
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        auto val = r.as_f64(v);
        if (!val) {
            return std::unexpected(val.error());
        }
        return static_cast<T>(*val);
    } else if constexpr (detail::is_transparent_struct_v<T>) {
        T out{};
        auto refs = refl::member_refs(out);
        auto& field = std::get<0>(refs);
        using field_t = remove_cvref_t<decltype(field)>;
        using value_t =
            std::conditional_t<detail::is_attr_v<field_t>, detail::attr_value_t<field_t>, field_t>;
        auto parsed = read_value<Reader, value_t>(r, v);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if constexpr (detail::is_attr_v<field_t>) {
            detail::assign_attr(field, std::move(*parsed));
        } else {
            field = std::move(*parsed);
        }
        return out;
    } else if constexpr (detail::is_reflectable_v<T>) {
        return read_object<Reader, T>(r, v);
    } else {
        static_assert(always_false_v<T>, "Unsupported type for deserialization.");
    }
}

}  // namespace detail

template <class Writer, class T>
void write(Writer& w, const T& value) {
    detail::write_value(w, value);
}

template <class Reader, class T>
result<T> read(Reader& r, value_type_t<Reader> v) {
    return detail::read_value<Reader, T>(r, v);
}

}  // namespace eventide::serde
