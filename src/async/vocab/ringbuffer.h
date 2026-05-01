#pragma once

#include <cstddef>
#include <vector>

namespace kota {

class ring_buffer {
public:
    explicit ring_buffer(std::size_t cap = 64 * 1024) : data(cap) {}

    std::size_t readable_bytes() const {
        return size;
    }

    std::size_t writable_bytes() const {
        return data.size() - size;
    }

    std::size_t read(char* dest, std::size_t len);

    std::pair<const char*, std::size_t> get_read_ptr() const;
    void advance_read(std::size_t len);

    std::pair<char*, std::size_t> get_write_ptr();
    void advance_write(std::size_t len);

private:
    std::vector<char> data;
    std::size_t head = 0;
    std::size_t tail = 0;
    std::size_t size = 0;
};

}  // namespace kota
