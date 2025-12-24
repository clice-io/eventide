#pragma once

#include <cstddef>
#include <memory>

namespace eventide {

class handle {
protected:
    handle(std::size_t size);

    ~handle();

public:
    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;

    handle(handle&& other) noexcept : data(other.data) {
        other.data = nullptr;
    }

    handle& operator=(handle&& other) {
        if(this == &other) [[unlikely]] {
            return *this;
        }

        this->~handle();
        return *new (this) handle(std::move(other));
    }

    template <typename T>
    T* as() {
        return static_cast<T*>(data);
    }

    template <typename T>
    const T* as() const {
        return static_cast<const T*>(data);
    }

    bool is_active();

    void ref();

    void unref();

private:
    handle() = default;

    void* data;
};

}  // namespace eventide
