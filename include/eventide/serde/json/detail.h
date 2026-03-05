#pragma once

#include "eventide/serde/detail/serialize_helpers.h"

namespace eventide::serde::json::detail {

/// Aliases for backwards compatibility.
template <typename S>
using SerializeArray = serde::detail::SerializeArray<S>;

template <typename S>
using SerializeObject = serde::detail::SerializeObject<S>;

}  // namespace eventide::serde::json::detail
