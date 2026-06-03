#pragma once

#include <string>

namespace kota {

class async_node;

std::string dump_dot(const async_node& root);

}  // namespace kota
