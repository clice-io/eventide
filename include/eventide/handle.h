#pragma once

#include <cstddef>

#include "atom.h"

namespace eventide {

template <typename Derived>
class handle : non_copyable, protected layout<Derived> {
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
