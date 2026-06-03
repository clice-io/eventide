#pragma once

#include <string>

namespace kota {

class async_node;

template <typename T, typename E, typename C>
class task;

std::string dump_dot(const async_node& root);

template <typename T, typename E, typename C>
std::string dump_dot(task<T, E, C>& t) {
    return dump_dot(*t.operator->());
}

}  // namespace kota
