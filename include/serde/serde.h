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

template <typename T>
concept annotated_field_type = requires {
    typename std::remove_cvref_t<T>::annotated_type;
    typename std::remove_cvref_t<T>::attrs;
};

struct attr_traits_base {
    constexpr static bool skip = false;
    constexpr static bool flatten = false;
    constexpr static bool rename = false;
    constexpr static bool enum_string = false;
    constexpr static std::string_view rename_name{};
    using enum_policy = void;

    constexpr static bool matches_alias(std::string_view) {
        return false;
    }

    template <typename Value>
    constexpr static bool should_skip(const Value&, bool) {
        return false;
    }
};

template <typename Attr>
struct attr_traits : attr_traits_base {
    constexpr static bool skip = std::is_same_v<Attr, attr::skip>;
    constexpr static bool flatten = std::is_same_v<Attr, attr::flatten>;
};

template <typename Policy>
struct attr_traits<attr::enum_string<Policy>> : attr_traits_base {
    constexpr static bool enum_string = true;
    using enum_policy = Policy;
};

template <fixed_string Name>
struct attr_traits<attr::rename<Name>> : attr_traits_base {
    constexpr static bool rename = true;
    constexpr static std::string_view rename_name = Name;
};

template <fixed_string... Names>
struct attr_traits<attr::alias<Names...>> : attr_traits_base {
    constexpr static bool matches_alias(std::string_view key_name) {
        for(auto alias_name: attr::alias<Names...>::names) {
            if(alias_name == key_name) {
                return true;
            }
        }
        return false;
    }
};

template <typename Pred>
struct attr_traits<attr::skip_if<Pred>> : attr_traits_base {
    template <typename Value>
    constexpr static bool should_skip(const Value& value, bool is_serialize) {
        if constexpr(requires {
                         { Pred{}(value, is_serialize) } -> std::convertible_to<bool>;
                     }) {
            return static_cast<bool>(Pred{}(value, is_serialize));
        } else if constexpr(requires {
                                { Pred{}(value) } -> std::convertible_to<bool>;
                            }) {
            return static_cast<bool>(Pred{}(value));
        } else {
            static_assert(
                dependent_false<Pred>,
                "attr::skip_if predicate must return bool and accept (const Value&, bool) or (const Value&)");
            return false;
        }
    }
};

template <typename... Ts>
struct first_non_void {
    using type = void;
};

template <typename T, typename... Ts>
struct first_non_void<T, Ts...> {
    using type = std::conditional_t<std::is_void_v<T>, typename first_non_void<Ts...>::type, T>;
};

template <typename... Ts>
using first_non_void_t = typename first_non_void<Ts...>::type;

template <typename AttrTuple>
struct annotated_attr_metadata;

template <typename... Attrs>
struct annotated_attr_metadata<std::tuple<Attrs...>> {
    constexpr static bool skip = (false || ... || attr_traits<Attrs>::skip);
    constexpr static bool flatten = (false || ... || attr_traits<Attrs>::flatten);
    constexpr static bool has_rename = (false || ... || attr_traits<Attrs>::rename);
    constexpr static bool enum_string = (false || ... || attr_traits<Attrs>::enum_string);
    using enum_policy = first_non_void_t<typename attr_traits<Attrs>::enum_policy...>;

    constexpr static std::string_view rename_name = []() constexpr {
        std::string_view out{};
        if constexpr(sizeof...(Attrs) > 0) {
            (([&] {
                 if constexpr(attr_traits<Attrs>::rename) {
                     out = attr_traits<Attrs>::rename_name;
                 }
             }()),
             ...);
        }
        return out;
    }();

    constexpr static std::string_view serialized_name(std::string_view default_name) {
        if constexpr(has_rename) {
            return rename_name;
        } else {
            return default_name;
        }
    }

    constexpr static bool matches_key(std::string_view key_name, std::string_view default_name) {
        bool rename_matched = false;
        if constexpr(sizeof...(Attrs) > 0) {
            (([&] {
                 if constexpr(attr_traits<Attrs>::rename) {
                     rename_matched =
                         rename_matched || (attr_traits<Attrs>::rename_name == key_name);
                 }
             }()),
             ...);
        }
        if(rename_matched) {
            return true;
        }

        const bool alias_matched = (false || ... || attr_traits<Attrs>::matches_alias(key_name));
        return alias_matched || (!has_rename && key_name == default_name);
    }

    template <typename Value>
    constexpr static bool should_skip_serialize(const Value& value) {
        if constexpr(skip) {
            return true;
        } else {
            return (false || ... || attr_traits<Attrs>::template should_skip<Value>(value, true));
        }
    }

    template <typename Value>
    constexpr static bool should_skip_deserialize(const Value& value) {
        if constexpr(skip) {
            return true;
        } else {
            return (false || ... || attr_traits<Attrs>::template should_skip<Value>(value, false));
        }
    }
};

template <annotated_field_type FieldType>
struct annotated_field_metadata :
    annotated_attr_metadata<typename std::remove_cvref_t<FieldType>::attrs> {};

template <annotated_field_type FieldType, typename Value>
constexpr decltype(auto) annotated_value(Value&& value) {
    using annotate_t = std::remove_cvref_t<FieldType>;
    using underlying_t = typename annotate_t::annotated_type;
    if constexpr(std::is_const_v<std::remove_reference_t<Value>>) {
        return static_cast<const underlying_t&>(value);
    } else {
        return static_cast<underlying_t&>(value);
    }
}

template <typename E, typename SerializeStruct, typename Field>
constexpr auto serialize_struct_field(SerializeStruct& s_struct, Field field)
    -> std::expected<void, E> {
    using field_t = typename std::remove_cvref_t<decltype(field)>::type;

    if constexpr(!annotated_field_type<field_t>) {
        return s_struct.serialize_field(field.name(), field.value());
    } else {
        using meta = annotated_field_metadata<field_t>;

        if constexpr(meta::skip) {
            return {};
        } else {
            auto&& value = annotated_value<field_t>(field.value());
            if(meta::should_skip_serialize(value)) {
                return {};
            }

            if constexpr(meta::flatten) {
                using nested_t = std::remove_cvref_t<decltype(value)>;
                static_assert(refl::reflectable_class<nested_t>,
                              "attr::flatten requires a reflectable class field type");

                std::expected<void, E> nested_result;
                refl::for_each(value, [&](auto nested_field) {
                    auto status = serialize_struct_field<E>(s_struct, nested_field);
                    if(!status) {
                        nested_result = std::unexpected(status.error());
                        return false;
                    }
                    return true;
                });
                return nested_result;
            }

            if constexpr(meta::enum_string) {
                using enum_t = std::remove_cvref_t<decltype(value)>;
                static_assert(std::is_enum_v<enum_t>,
                              "attr::enum_string requires an enum field type");

                auto enum_text =
                    serde::detail::map_enum_to_string<enum_t, typename meta::enum_policy>(value);
                return s_struct.serialize_field(meta::serialized_name(field.name()), enum_text);
            }

            return s_struct.serialize_field(meta::serialized_name(field.name()), value);
        }
    }
}

template <typename E, typename DeserializeStruct, typename Field>
constexpr auto deserialize_struct_field(DeserializeStruct& d_struct,
                                        std::string_view key_name,
                                        Field field) -> std::expected<bool, E> {
    using field_t = typename std::remove_cvref_t<decltype(field)>::type;

    if constexpr(!annotated_field_type<field_t>) {
        if(field.name() != key_name) {
            return false;
        }

        auto result = d_struct.deserialize_value(field.value());
        if(!result) {
            return std::unexpected(result.error());
        }
        return true;
    } else {
        using meta = annotated_field_metadata<field_t>;

        if constexpr(meta::skip) {
            return false;
        } else {
            auto&& value = annotated_value<field_t>(field.value());
            if constexpr(meta::flatten) {
                if(meta::should_skip_deserialize(value)) {
                    return false;
                }

                using nested_t = std::remove_cvref_t<decltype(value)>;
                static_assert(refl::reflectable_class<nested_t>,
                              "attr::flatten requires a reflectable class field type");

                std::expected<void, E> nested_error;
                bool matched = false;
                refl::for_each(value, [&](auto nested_field) {
                    auto status = deserialize_struct_field<E>(d_struct, key_name, nested_field);
                    if(!status) {
                        nested_error = std::unexpected(status.error());
                        return false;
                    }
                    if(*status) {
                        matched = true;
                        return false;
                    }
                    return true;
                });
                if(!nested_error) {
                    return std::unexpected(nested_error.error());
                }
                return matched;
            }

            if(!meta::matches_key(key_name, field.name())) {
                return false;
            }

            if(meta::should_skip_deserialize(value)) {
                auto skipped = d_struct.skip_value();
                if(!skipped) {
                    return std::unexpected(skipped.error());
                }
                return true;
            }

            if constexpr(meta::enum_string) {
                using enum_t = std::remove_cvref_t<decltype(value)>;
                static_assert(std::is_enum_v<enum_t>,
                              "attr::enum_string requires an enum field type");

                std::string enum_text;
                auto result = d_struct.deserialize_value(enum_text);
                if(!result) {
                    return std::unexpected(result.error());
                }

                auto parsed = serde::detail::map_string_to_enum<enum_t, typename meta::enum_policy>(
                    enum_text);
                if(parsed.has_value()) {
                    value = *parsed;
                } else {
                    value = enum_t{};
                }
                return true;
            }

            auto result = d_struct.deserialize_value(value);
            if(!result) {
                return std::unexpected(result.error());
            }
            return true;
        }
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
    } else if constexpr(detail::annotated_field_type<V>) {
        using meta = detail::annotated_field_metadata<V>;
        auto&& value = detail::annotated_value<V>(v);

        if constexpr(meta::skip || meta::flatten) {
            static_assert(!meta::skip && !meta::flatten,
                          "skip/flatten attributes are only valid for struct fields");
            return s.serialize_none();
        } else if constexpr(meta::enum_string) {
            using enum_t = std::remove_cvref_t<decltype(value)>;
            static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");
            auto enum_text =
                serde::detail::map_enum_to_string<enum_t, typename meta::enum_policy>(value);
            return s.serialize_str(enum_text);
        } else {
            return serialize(s, value);
        }
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
    } else if constexpr(std::same_as<V, std::nullptr_t>) {
        return s.serialize_none();
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
        refl::for_each(v, [&](auto field) {
            auto result = detail::serialize_struct_field<E>(*s_struct, field);
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
    } else if constexpr(detail::annotated_field_type<V>) {
        using meta = detail::annotated_field_metadata<V>;
        auto&& value = detail::annotated_value<V>(v);

        if constexpr(meta::skip || meta::flatten) {
            static_assert(!meta::skip && !meta::flatten,
                          "skip/flatten attributes are only valid for struct fields");
            return d.skip_value();
        } else if constexpr(meta::enum_string) {
            using enum_t = std::remove_cvref_t<decltype(value)>;
            static_assert(std::is_enum_v<enum_t>, "attr::enum_string requires an enum field type");

            std::string enum_text;
            auto parsed = d.deserialize_str(enum_text);
            if(!parsed) {
                return std::unexpected(parsed.error());
            }

            auto mapped =
                serde::detail::map_string_to_enum<enum_t, typename meta::enum_policy>(enum_text);
            if(mapped.has_value()) {
                value = *mapped;
            } else {
                value = enum_t{};
            }
            return {};
        } else {
            return deserialize(d, value);
        }
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
    } else if constexpr(std::same_as<V, std::string> || std::derived_from<V, std::string>) {
        return d.deserialize_str(static_cast<std::string&>(v));
    } else if constexpr(std::same_as<V, std::vector<std::byte>>) {
        return d.deserialize_bytes(v);
    } else if constexpr(std::same_as<V, std::nullptr_t>) {
        auto is_none = d.deserialize_none();
        if(!is_none) {
            return std::unexpected(is_none.error());
        }
        if(*is_none) {
            v = nullptr;
            return {};
        }
        return std::unexpected(E{});
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
                serde::spelling::parseable_map_key<key_t>,
                "auto map deserialization requires key_type parseable from JSON object keys");
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

                auto parsed_key = serde::spelling::parse_map_key<key_t>(**key);
                if(!parsed_key) {
                    if constexpr(requires { d_map->invalid_key(**key); }) {
                        auto invalid = d_map->invalid_key(**key);
                        if(!invalid) {
                            return std::unexpected(invalid.error());
                        }
                        continue;
                    } else if constexpr(std::default_initializable<E>) {
                        return std::unexpected(E{});
                    } else {
                        static_assert(
                            dependent_false<key_t>,
                            "key parse failed and deserializer does not provide invalid_key");
                    }
                }

                mapped_t mapped{};
                auto mapped_status = d_map->deserialize_value(mapped);
                if(!mapped_status) {
                    return std::unexpected(mapped_status.error());
                }

                detail::insert_map_entry(v, std::move(*parsed_key), std::move(mapped));
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

            refl::for_each(v, [&](auto field) {
                auto status = detail::deserialize_struct_field<E>(*d_struct, key_name, field);
                if(!status) {
                    field_result = std::unexpected(status.error());
                    return false;
                }
                if(*status) {
                    matched = true;
                }
                return !matched;
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
