#pragma once

#include "attrs.h"
#include "traits.h"
#include "../reflection/enum.h"
#include "../reflection/struct.h"

namespace serde {

template <typename S, typename T>
struct serialize_traits;

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
    } else if constexpr(char_like<V>) {
        return s.serialize_char(v);
    } else if constexpr(str_like<V>) {
        return s.serialize_str(v);
    } else if constexpr(bytes_like<V>) {
        return s.serialize_bytes(v);
    } else if constexpr(is_specialization_of<std::optional, V>) {
        if(v.has_value()) {
            return s.serialize_none();
        } else {
            return s.serialize_some(v.value());
        }
    } else if constexpr(is_specialization_of<std::variant, T>) {
        return std::visit([&](auto&& value) { return s.serialize_some(value); }, v);
    } else if constexpr(std::ranges::input_range<V>) {
        constexpr auto kind = format_kind<V>;
        if constexpr(kind == range_format::sequence || kind == range_format::set) {
            auto s_seq = s.serialize_seq(std::ranges::size(v));
            if(!s_seq) {
                return std::unexpected(s_seq.error());
            }

            for(auto& e: v) {
                auto element = s_seq->serialize_element(e);
                if(!element) {
                    return std::unexpected(element.error());
                }
            }

            return s_seq->end();
        } else if constexpr(kind == range_format::map) {
            auto s_map = s.serialize_map(std::ranges::size(v));
            if(!s_map) {
                return std::unexpected(s_map.error());
            }

            for(auto& [key, value]: v) {
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

}  // namespace serde
