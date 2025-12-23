#include "eventide/ringbuffer.h"

#include <algorithm>
#include <cstring>

namespace eventide {

size_t ring_buffer::read(char* dest, size_t len) {
    const size_t to_read = std::min(len, size);
    if(to_read == 0) {
        return 0;
    }

    const size_t first_chunk = std::min(to_read, data.size() - head);
    std::memcpy(dest, data.data() + head, first_chunk);

    const size_t remaining = to_read - first_chunk;
    if(remaining > 0) {
        std::memcpy(dest + first_chunk, data.data(), remaining);
    }

    head = (head + to_read) % data.size();
    size -= to_read;
    return to_read;
}

std::pair<char*, size_t> ring_buffer::get_write_ptr() {
    if(data.empty()) {
        return {nullptr, 0};
    }

    const size_t writable = writable_bytes();
    size_t contiguous = 0;
    if(writable == 0) {
        contiguous = 0;
    } else if(tail >= head) {
        contiguous = std::min(writable, data.size() - tail);
    } else {
        contiguous = head - tail;
    }

    return {data.data() + tail, contiguous};
}

void ring_buffer::advance_write(size_t len) {
    const size_t writable = writable_bytes();
    if(len > writable) {
        len = writable;
    }

    tail = (tail + len) % data.size();
    size += len;
}

}  // namespace eventide
