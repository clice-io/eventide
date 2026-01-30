#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace eventide {

class handle {
protected:
    handle() noexcept : data(nullptr) {}

    handle(std::size_t size) noexcept;

    ~handle() noexcept;

public:
    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;

    handle(handle&& other) noexcept : data(other.data) {
        other.data = nullptr;
    }

    handle& operator=(handle&& other) noexcept {
        if(this == &other) [[unlikely]] {
            return *this;
        }

        this->~handle();
        return *new (this) handle(std::move(other));
    }

    template <typename T>
    T* as() noexcept {
        return static_cast<T*>(raw_data());
    }

    template <typename T>
    const T* as() const noexcept {
        return static_cast<const T*>(raw_data());
    }

    /// Mark the underlying storage as successfully initialized.
    void mark_initialized() noexcept;

    bool initialized() const noexcept;

    bool is_active() const noexcept;

    void ref() noexcept;

    void unref() noexcept;

private:
    void* raw_data() noexcept;

    const void* raw_data() const noexcept;

    void* data;
};

}  // namespace eventide
