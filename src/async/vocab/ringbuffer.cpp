#include "ringbuffer.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace kota {

std::size_t ring_buffer::read(char* dest, std::size_t len) {
    const std::size_t to_read = std::min(len, size);
    if(to_read == 0) {
        return 0;
    }

    const std::size_t first_chunk = std::min(to_read, data.size() - head);
    std::memcpy(dest, data.data() + head, first_chunk);

    const std::size_t remaining = to_read - first_chunk;
    if(remaining > 0) {
        std::memcpy(dest + first_chunk, data.data(), remaining);
    }

    head = (head + to_read) % data.size();
    size -= to_read;
    return to_read;
}

std::pair<const char*, std::size_t> ring_buffer::get_read_ptr() const {
    if(size == 0 || data.empty()) {
        return {nullptr, 0};
    }

    // When the buffer is full, head == tail but size > 0. Using `>=` here
    // would yield contiguous = 0, causing read_chunk() to return an empty
    // span and the caller to spin forever. Use strict `>` so the full case
    // falls through to the else branch (data.size() - head), which is correct.
    std::size_t contiguous = 0;
    if(tail > head) {
        contiguous = tail - head;
    } else {
        contiguous = data.size() - head;
    }

    assert(contiguous > 0 && "get_read_ptr: non-empty buffer must yield contiguous > 0");
    return {data.data() + head, contiguous};
}

void ring_buffer::advance_read(std::size_t len) {
    if(len > size) {
        len = size;
    }

    if(len == 0 || data.empty()) {
        return;
    }

    head = (head + len) % data.size();
    size -= len;
}

std::pair<char*, std::size_t> ring_buffer::get_write_ptr() {
    if(data.empty()) {
        return {nullptr, 0};
    }

    const std::size_t writable = writable_bytes();
    std::size_t contiguous = 0;
    if(writable == 0) {
        contiguous = 0;
    } else if(tail >= head) {
        contiguous = std::min(writable, data.size() - tail);
    } else {
        contiguous = head - tail;
    }

    return {data.data() + tail, contiguous};
}

void ring_buffer::advance_write(std::size_t len) {
    const std::size_t writable = writable_bytes();
    if(len > writable) {
        len = writable;
    }

    if(len == 0 || data.empty()) {
        return;
    }

    tail = (tail + len) % data.size();
    size += len;
}

}  // namespace kota
