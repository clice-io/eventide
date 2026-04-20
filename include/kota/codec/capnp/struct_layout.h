#pragma once

// Cap'n Proto's composite-list element requirement: the struct must be a
// trivial, standard-layout aggregate with no annotated fields. This is
// exactly the same predicate the flatbuffers backend uses for its inline
// struct path, so we simply re-use it under the capnp namespace.

#include "kota/codec/flatbuffers/struct_layout.h"

namespace kota::codec::capnp {

template <typename T>
constexpr bool can_inline_struct_v = flatbuffers::can_inline_struct_v<T>;

}  // namespace kota::codec::capnp
