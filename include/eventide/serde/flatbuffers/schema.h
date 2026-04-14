#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#include "eventide/serde/schema/virtual_schema.h"
#include "eventide/serde/serde/utils/common.h"

namespace eventide::serde::flatbuffers {

namespace schema_detail {

using serde::detail::remove_optional_t;

template <typename T>
constexpr bool is_scalar_field_v =
    std::same_as<T, bool> || serde::int_like<T> || serde::uint_like<T> || serde::floating_like<T>;

template <typename T>
struct schema_struct_trait;

template <typename T>
constexpr bool is_schema_struct_field_v = [] {
    using U = std::remove_cvref_t<remove_optional_t<T>>;
    if constexpr(serde::annotated_type<U>) {
        return false;
    } else if constexpr(is_scalar_field_v<U> || std::is_enum_v<U>) {
        return true;
    } else if constexpr(refl::reflectable_class<U>) {
        return schema_struct_trait<U>::value;
    } else {
        return false;
    }
}();

template <typename T>
struct schema_struct_trait {
    static consteval bool fields_supported() {
        if constexpr(!refl::reflectable_class<T>) {
            return false;
        } else {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (is_schema_struct_field_v<refl::field_type<T, I>> && ...);
            }(std::make_index_sequence<refl::field_count<T>()>{});
        }
    }

    constexpr static bool value = refl::reflectable_class<T> && std::is_trivial_v<T> &&
                                  std::is_standard_layout_v<T> && fields_supported();
};

template <typename T>
constexpr bool is_schema_struct_v = schema_struct_trait<T>::value;

template <typename T>
constexpr bool is_string_like_field_v = std::same_as<remove_optional_t<T>, std::string> ||
                                        std::same_as<remove_optional_t<T>, std::string_view>;

}  // namespace schema_detail

template <typename T>
constexpr bool is_schema_struct_v = schema_detail::is_schema_struct_v<T>;

template <typename T>
consteval bool has_annotated_fields() {
    using U = std::remove_cvref_t<T>;
    if constexpr(!refl::reflectable_class<U>) {
        return false;
    } else {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return (serde::annotated_type<refl::field_type<U, I>> || ...);
        }(std::make_index_sequence<refl::field_count<U>()>{});
    }
}

template <typename T>
constexpr bool can_inline_struct_v =
    refl::reflectable_class<T> && is_schema_struct_v<T> && !has_annotated_fields<T>();

template <typename Key, typename Value>
struct map_entry {
    Key key{};
    Value value{};
};

template <typename Map>
auto to_sorted_entries(const Map& map)
    -> std::vector<map_entry<typename Map::key_type, typename Map::mapped_type>> {
    using key_t = typename Map::key_type;
    using mapped_t = typename Map::mapped_type;

    std::vector<map_entry<key_t, mapped_t>> entries;
    entries.reserve(map.size());
    for(const auto& [key, value]: map) {
        entries.push_back(map_entry<key_t, mapped_t>{key, value});
    }

    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.key < rhs.key;
    });
    return entries;
}

template <typename EntryVec, typename Key>
auto bsearch_entry(const EntryVec& entries, const Key& key) -> const
    typename EntryVec::value_type* {
    auto it =
        std::lower_bound(entries.begin(), entries.end(), key, [](const auto& entry, const auto& k) {
            return entry.key < k;
        });
    if(it == entries.end() || it->key != key) {
        return nullptr;
    }
    return &(*it);
}

}  // namespace eventide::serde::flatbuffers
