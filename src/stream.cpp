#include "eventide/stream.h"

namespace eventide {

template <typename Derived>
void stream<Derived>::read(std::span<char> buf) {}

template <typename Derived>
void stream<Derived>::write(std::span<const char> data) {}

template class stream<pipe>;
template class stream<tcp_socket>;
template class stream<console>;

}  // namespace eventide
