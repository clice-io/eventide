#pragma once

#include <expected>
#include <system_error>

namespace eventide {

std::error_code uv_error(int errc);

template <typename T>
using result = std::expected<T, std::error_code>;

struct cancellation_t {};

template <typename T>
constexpr bool is_cancellation_t = false;

template <typename T>
constexpr bool is_cancellation_t<std::expected<T, cancellation_t>> = true;

template <typename T>
class task;

template <typename T>
using ctask = task<std::expected<T, cancellation_t>>;

}  // namespace eventide
