#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "kota/support/ranges.h"
#include "kota/support/type_traits.h"
#include "kota/meta/type_kind.h"

namespace kota::codec {

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
