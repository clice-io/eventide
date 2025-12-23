#pragma once

#include "eventide_uv_layout.hpp"

namespace eventide {

struct non_copyable {
    non_copyable() = default;
    ~non_copyable() = default;

    non_copyable(const non_copyable&) = delete;
    non_copyable& operator=(const non_copyable&) = delete;

    non_copyable(non_copyable&&) = default;
    non_copyable& operator=(non_copyable&&) = default;
};

struct non_moveable {
    non_moveable() = default;
    ~non_moveable() = default;

    non_moveable(const non_moveable&) = delete;
    non_moveable& operator=(const non_moveable&) = delete;

    non_moveable(non_moveable&&) = delete;
    non_moveable& operator=(non_moveable&&) = delete;
};

}  // namespace eventide
