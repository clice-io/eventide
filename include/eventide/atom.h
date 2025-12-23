#pragma once

#include "eventide_uv_layout.hpp"

namespace eventide {

template <typename T>
class uv_layout : layout<T> {
public:
    void* native_handle() {
        return storage;
    }

    const void* native_handle() const {
        return storage;
    }

    uv_layout(const uv_layout&) = delete;
    uv_layout& operator=(const uv_layout&) = delete;

    uv_layout(uv_layout&&) = delete;
    uv_layout& operator=(uv_layout&&) = delete;

public:
    uv_layout() = default;

    alignas(layout<T>::align) char storage[layout<T>::size];
};

}  // namespace eventide
