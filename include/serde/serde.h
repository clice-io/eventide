#pragma once

#include "attrs.h"
#include "traits.h"
#include "../reflection/enum.h"
#include "../reflection/struct.h"

namespace serde {

template <typename S, typename T>
struct serialize_traits;

template <typename D, typename T>
struct deserialize_traits;

namespace detail {

template <typename Container, typename Element>
concept sequence_insertable = requires(Container& container, Element&& element) {
    container.emplace_back(std::forward<Element>(element));
} || requires(Container& container, Element&& element) {
    container.push_back(std::forward<Element>(element));
} || requires(Container& container, Element&& element) {
    container.insert(container.end(), std::forward<Element>(element));
} || requires(Container& container, Element&& element) {
    container.insert(std::forward<Element>(element));
};

template <typename Container, typename Element>
constexpr bool append_sequence_element(Container& container, Element&& element) {
    if constexpr(requires { container.emplace_back(std::forward<Element>(element)); }) {
        container.emplace_back(std::forward<Element>(element));
        return true;
    } else if constexpr(requires { container.push_back(std::forward<Element>(element)); }) {
        container.push_back(std::forward<Element>(element));
        return true;
    } else if constexpr(requires {
                            container.insert(container.end(), std::forward<Element>(element));
                        }) {
        container.insert(container.end(), std::forward<Element>(element));
        return true;
    } else if constexpr(requires { container.insert(std::forward<Element>(element)); }) {
        container.insert(std::forward<Element>(element));
        return true;
    } else {
        return false;
    }
}

template <typename Map, typename Key, typename Mapped>
concept map_insertable = requires(Map& map, Key&& key, Mapped&& value) {
    map.insert_or_assign(std::forward<Key>(key), std::forward<Mapped>(value));
} || requires(Map& map, Key&& key, Mapped&& value) {
    map.emplace(std::forward<Key>(key), std::forward<Mapped>(value));
} || requires(Map& map, Key&& key, Mapped&& value) {
    map.insert(typename Map::value_type{std::forward<Key>(key), std::forward<Mapped>(value)});
};

template <typename Map, typename Key, typename Mapped>
constexpr bool insert_map_entry(Map& map, Key&& key, Mapped&& value) {
    if constexpr(requires {
                     map.insert_or_assign(std::forward<Key>(key), std::forward<Mapped>(value));
                 }) {
        map.insert_or_assign(std::forward<Key>(key), std::forward<Mapped>(value));
        return true;
    } else if constexpr(requires {
                            map.emplace(std::forward<Key>(key), std::forward<Mapped>(value));
                        }) {
        map.emplace(std::forward<Key>(key), std::forward<Mapped>(value));
        return true;
    } else if constexpr(requires {
                            map.insert(typename Map::value_type{std::forward<Key>(key),
                                                                std::forward<Mapped>(value)});
                        }) {
        map.insert(typename Map::value_type{std::forward<Key>(key), std::forward<Mapped>(value)});
        return true;
    } else {
        return false;
    }
}

}  // namespace detail

template <serializer_like S,
          typename V,
          typename T = typename S::value_type,
          typename E = S::error_type>
constexpr auto serialize(S& s, const V& v) -> std::expected<T, E> {
    using Serde = serialize_traits<S, V>;

    if constexpr(requires { Serde::serialize(s, v); }) {
        return Serde::serialize(s, v);
    } else if constexpr(bool_like<V>) {
        return s.serialize_bool(v);
    } else if constexpr(int_like<V>) {
        return s.serialize_int(v);
    } else if constexpr(uint_like<V>) {
        return s.serialize_uint(v);
    } else if constexpr(floating_like<V>) {
        return s.serialize_float(static_cast<double>(v));
    } else if constexpr(char_like<V>) {
        return s.serialize_char(v);
    } else if constexpr(str_like<V>) {
        return s.serialize_str(v);
    } else if constexpr(bytes_like<V>) {
        return s.serialize_bytes(v);
    } else if constexpr(is_specialization_of<std::optional, V>) {
        if(v.has_value()) {
            return s.serialize_some(v.value());
        } else {
            return s.serialize_none();
        }
    } else if constexpr(is_specialization_of<std::variant, V>) {
        return std::visit([&](auto&& value) { return s.serialize_some(value); }, v);
    } else if constexpr(std::ranges::input_range<V>) {
        constexpr auto kind = format_kind<V>;
        if constexpr(kind == range_format::sequence || kind == range_format::set) {
            std::optional<std::size_t> len = std::nullopt;
            if constexpr(std::ranges::sized_range<V>) {
                len = static_cast<std::size_t>(std::ranges::size(v));
            }

            auto s_seq = s.serialize_seq(len);
            if(!s_seq) {
                return std::unexpected(s_seq.error());
            }

            for(auto&& e: v) {
                auto element = s_seq->serialize_element(e);
                if(!element) {
                    return std::unexpected(element.error());
                }
            }

            return s_seq->end();
        } else if constexpr(kind == range_format::map) {
            std::optional<std::size_t> len = std::nullopt;
            if constexpr(std::ranges::sized_range<V>) {
                len = static_cast<std::size_t>(std::ranges::size(v));
            }

            auto s_map = s.serialize_map(len);
            if(!s_map) {
                return std::unexpected(s_map.error());
            }

            for(auto&& [key, value]: v) {
                auto entry = s_map->serialize_entry(key, value);
                if(!entry) {
                    return std::unexpected(entry.error());
                }
            }

            return s_map->end();
        } else {
            static_assert(dependent_false<V>, "cannot auto serialize the input range");
        }
    } else if constexpr(is_pair_v<V> || is_tuple_v<V>) {
        auto s_tuple = s.serialize_tuple(std::tuple_size_v<V>);
        if(!s_tuple) {
            return std::unexpected(s_tuple.error());
        }

        std::expected<void, E> element_result;
        auto for_each = [&](const auto& element) -> bool {
            auto result = s_tuple->serialize_element(element);
            if(!result) {
                element_result = std::unexpected(result.error());
                return false;
            }
            return true;
        };
        std::apply([&](const auto&... elements) { return (for_each(elements) && ...); }, v);
        if(!element_result) {
            return std::unexpected(element_result.error());
        }

        return s_tuple->end();
    } else if constexpr(refl::reflectable_class<V>) {
        auto s_struct = s.serialize_struct(refl::type_name<V>(), refl::field_count<V>());
        if(!s_struct) {
            return std::unexpected(s_struct.error());
        }

        std::expected<void, E> field_result;
        refl::for_each(v, [&](auto&& field) {
            auto result = s_struct->serialize_field(field.name(), field.value());
            if(!result) {
                field_result = std::unexpected(result.error());
                return false;
            }
            return true;
        });
        if(!field_result) {
            return std::unexpected(field_result.error());
        }

        return s_struct->end();
    } else {
        static_assert(dependent_false<V>,
                      "cannot auto serialize the value, try to specialize for it");
    }
}

template <deserializer_like D, typename V, typename E = typename D::error_type>
constexpr auto deserialize(D& d, V& v) -> std::expected<void, E> {
    using Deserde = deserialize_traits<D, V>;

    if constexpr(requires { Deserde::deserialize(d, v); }) {
        return Deserde::deserialize(d, v);
    } else if constexpr(bool_like<V>) {
        return d.deserialize_bool(v);
    } else if constexpr(int_like<V>) {
        return d.deserialize_int(v);
    } else if constexpr(uint_like<V>) {
        return d.deserialize_uint(v);
    } else if constexpr(floating_like<V>) {
        return d.deserialize_float(v);
    } else if constexpr(char_like<V>) {
        return d.deserialize_char(v);
    } else if constexpr(std::same_as<V, std::string>) {
        return d.deserialize_str(v);
    } else if constexpr(std::same_as<V, std::vector<std::byte>>) {
        return d.deserialize_bytes(v);
    } else if constexpr(is_specialization_of<std::optional, V>) {
        auto is_none = d.deserialize_none();
        if(!is_none) {
            return std::unexpected(is_none.error());
        }

        if(*is_none) {
            v.reset();
            return {};
        }

        using value_t = typename V::value_type;
        if(v.has_value()) {
            return d.deserialize_some(v.value());
        }

        if constexpr(std::default_initializable<value_t>) {
            v.emplace();
            auto status = d.deserialize_some(v.value());
            if(!status) {
                v.reset();
                return std::unexpected(status.error());
            }
            return {};
        } else {
            static_assert(dependent_false<V>,
                          "cannot auto deserialize optional<T> without default-constructible T");
        }
    } else if constexpr(is_specialization_of<std::variant, V>) {
        return std::visit([&](auto& value) { return d.deserialize_some(value); }, v);
    } else if constexpr(std::ranges::input_range<V>) {
        constexpr auto kind = format_kind<V>;
        if constexpr(kind == range_format::sequence || kind == range_format::set) {
            auto d_seq = d.deserialize_seq(std::nullopt);
            if(!d_seq) {
                return std::unexpected(d_seq.error());
            }

            if constexpr(requires { v.clear(); }) {
                v.clear();
            }

            using element_t = std::ranges::range_value_t<V>;
            static_assert(
                std::default_initializable<element_t>,
                "auto deserialization for ranges requires default-constructible elements");
            static_assert(detail::sequence_insertable<V, element_t>,
                          "cannot auto deserialize range: container does not support insertion");

            while(true) {
                auto has_next = d_seq->has_next();
                if(!has_next) {
                    return std::unexpected(has_next.error());
                }
                if(!*has_next) {
                    break;
                }

                element_t element{};
                auto element_status = d_seq->deserialize_element(element);
                if(!element_status) {
                    return std::unexpected(element_status.error());
                }

                detail::append_sequence_element(v, std::move(element));
            }

            return d_seq->end();
        } else if constexpr(kind == range_format::map) {
            using key_t = typename V::key_type;
            using mapped_t = typename V::mapped_type;

            auto d_map = d.deserialize_map(std::nullopt);
            if(!d_map) {
                return std::unexpected(d_map.error());
            }

            if constexpr(requires { v.clear(); }) {
                v.clear();
            }

            static_assert(
                std::constructible_from<key_t, std::string_view>,
                "auto map deserialization requires key_type constructible from std::string_view");
            static_assert(std::default_initializable<mapped_t>,
                          "auto map deserialization requires default-constructible mapped_type");
            static_assert(detail::map_insertable<V, key_t, mapped_t>,
                          "cannot auto deserialize map: container does not support map insertion");

            while(true) {
                auto key = d_map->next_key();
                if(!key) {
                    return std::unexpected(key.error());
                }
                if(!key->has_value()) {
                    break;
                }

                mapped_t mapped{};
                auto mapped_status = d_map->deserialize_value(mapped);
                if(!mapped_status) {
                    return std::unexpected(mapped_status.error());
                }

                detail::insert_map_entry(v, key_t(**key), std::move(mapped));
            }

            return d_map->end();
        } else {
            static_assert(dependent_false<V>, "cannot auto deserialize the input range");
        }
    } else if constexpr(is_pair_v<V> || is_tuple_v<V>) {
        auto d_tuple = d.deserialize_tuple(std::tuple_size_v<V>);
        if(!d_tuple) {
            return std::unexpected(d_tuple.error());
        }

        std::expected<void, E> element_result;
        auto read_element = [&](auto& element) -> bool {
            auto result = d_tuple->deserialize_element(element);
            if(!result) {
                element_result = std::unexpected(result.error());
                return false;
            }
            return true;
        };
        std::apply([&](auto&... elements) { return (read_element(elements) && ...); }, v);
        if(!element_result) {
            return std::unexpected(element_result.error());
        }

        return d_tuple->end();
    } else if constexpr(refl::reflectable_class<V>) {
        auto d_struct = d.deserialize_struct(refl::type_name<V>(), refl::field_count<V>());
        if(!d_struct) {
            return std::unexpected(d_struct.error());
        }

        while(true) {
            auto key = d_struct->next_key();
            if(!key) {
                return std::unexpected(key.error());
            }
            if(!key->has_value()) {
                break;
            }

            std::string_view key_name = **key;
            bool matched = false;
            std::expected<void, E> field_result;

            refl::for_each(v, [&](auto&& field) {
                if(field.name() != key_name) {
                    return true;
                }

                matched = true;
                auto result = d_struct->deserialize_value(field.value());
                if(!result) {
                    field_result = std::unexpected(result.error());
                }
                return false;
            });

            if(!field_result) {
                return std::unexpected(field_result.error());
            }

            if(!matched) {
                auto skipped = d_struct->skip_value();
                if(!skipped) {
                    return std::unexpected(skipped.error());
                }
            }
        }

        return d_struct->end();
    } else {
        static_assert(dependent_false<V>,
                      "cannot auto deserialize the value, try to specialize for it");
    }
}

}  // namespace serde
