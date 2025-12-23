#pragma once

#include <cstddef>

#include "atom.h"

namespace eventide {

template <typename Derived>
class handle : public uv_layout<Derived> {
public:
    bool is_active() const;

    void close();

    void ref();

    void unref();

protected:
    handle() = default;

    ~handle() {
        close();
    }
};

}  // namespace eventide
