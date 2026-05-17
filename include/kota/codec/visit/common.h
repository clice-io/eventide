#pragma once

#include <optional>
#include <string>
#include <type_traits>

#include "kota/support/ranges.h"
#include "kota/support/type_traits.h"
#include "kota/meta/type_kind.h"

namespace kota::codec {

struct RawValue {
    std::string data;

    bool empty() const noexcept {
        return data.empty();
    }
};

namespace detail {

template <typename T>
struct remove_annotation {
    using type = std::remove_cvref_t<T>;
};

template <typename T>
    requires requires { typename std::remove_cvref_t<T>::annotated_type; }
struct remove_annotation<T> {
    using type = std::remove_cvref_t<typename std::remove_cvref_t<T>::annotated_type>;
};

template <typename T>
using remove_annotation_t = typename remove_annotation<T>::type;

template <typename T>
struct remove_optional {
    using type = std::remove_cvref_t<T>;
};

template <typename T>
struct remove_optional<std::optional<T>> {
    using type = std::remove_cvref_t<T>;
};

template <typename T>
using remove_optional_t = typename remove_optional<std::remove_cvref_t<T>>::type;

template <typename T>
using clean_t = remove_optional_t<remove_annotation_t<T>>;

}  // namespace detail

template <typename T>
concept null_like = meta::null_like<T>;

template <typename T>
concept bool_like = meta::bool_like<T>;

template <typename T>
concept int_like = meta::int_like<T>;

template <typename T>
concept uint_like = meta::uint_like<T>;

template <typename T>
concept floating_like = meta::floating_like<T>;

template <typename T>
concept char_like = meta::char_like<T>;

template <typename T>
concept str_like = meta::str_like<T>;

template <typename T>
concept bytes_like = meta::bytes_like<T>;

template <typename T>
constexpr inline bool is_pair_v = meta::is_pair_v<T>;

template <typename T>
constexpr inline bool is_tuple_v = meta::is_tuple_v<T>;

template <typename T>
concept tuple_like = meta::tuple_like<T>;

}  // namespace kota::codec
