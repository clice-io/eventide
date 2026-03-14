#pragma once

#include <array>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "annotation.h"
#include "attrs.h"
#include "config.h"
#include "traits.h"
#include "eventide/common/expected_try.h"
#include "eventide/common/ranges.h"
#include "eventide/reflection/enum.h"
#include "eventide/reflection/struct.h"
#include "eventide/serde/serde/attrs/behavior.h"
#include "eventide/serde/serde/attrs/schema.h"
#include "eventide/serde/serde/schema/schema.h"
#include "eventide/serde/serde/utils/apply_behavior.h"
#include "eventide/serde/serde/utils/common.h"
#include "eventide/serde/serde/utils/field_dispatch.h"
#include "eventide/serde/serde/utils/fwd.h"
#include "eventide/serde/serde/utils/reflectable.h"
#include "eventide/serde/serde/utils/tagged.h"

namespace eventide::serde {

template <serializer_like S, typename V, typename T, typename E>
constexpr auto serialize(S& s, const V& v) -> std::expected<T, E> {
    using Serde = serialize_traits<S, V>;

    if constexpr(requires { Serde::serialize(s, v); }) {
        return Serde::serialize(s, v);
    } else if constexpr(annotated_type<V>) {
        using attrs_t = typename std::remove_cvref_t<V>::attrs;
        auto&& value = annotated_value(v);
        using value_t = std::remove_cvref_t<decltype(value)>;

        // Field-only attrs at value level are errors
        static_assert(!detail::tuple_has_v<attrs_t, schema::skip>,
                      "schema::skip is only valid for struct fields");
        static_assert(!detail::tuple_has_v<attrs_t, schema::flatten>,
                      "schema::flatten is only valid for struct fields");

        // Tagged variant dispatch
        if constexpr(is_specialization_of<std::variant, value_t> &&
                     detail::tuple_any_of_v<attrs_t, is_tagged_attr>) {
            using tag_attr = detail::tuple_find_t<attrs_t, is_tagged_attr>;
            constexpr auto strategy = tagged_strategy_of<tag_attr>;
            if constexpr(strategy == tagged_strategy::external) {
                static_assert(variant_support<S>::externally_tagged,
                              "this serializer does not support externally tagged variants");
                return detail::serialize_externally_tagged<E>(s, value, tag_attr{});
            } else if constexpr(strategy == tagged_strategy::internal) {
                static_assert(variant_support<S>::internally_tagged,
                              "this serializer does not support internally tagged variants");
                return detail::serialize_internally_tagged<E>(s, value, tag_attr{});
            } else {
                static_assert(variant_support<S>::adjacently_tagged,
                              "this serializer does not support adjacently tagged variants");
                return detail::serialize_adjacently_tagged<E>(s, value, tag_attr{});
            }
        }
        // Behavior: with/as/enum_string — delegate to apply_serialize_behavior
        else if constexpr(detail::tuple_count_of_v<attrs_t, is_behavior_provider> > 0) {
            return *detail::apply_serialize_behavior<attrs_t, value_t, E>(
                value,
                [&](const auto& v) { return serialize(s, v); },
                [&](auto tag, const auto& v) {
                    using Adapter = typename decltype(tag)::type;
                    return Adapter::serialize(s, v);
                });
        }
        // Struct-level schema attrs for annotated structs
        else if constexpr(refl::reflectable_class<value_t> &&
                          (detail::tuple_has_spec_v<attrs_t, schema::rename_all> ||
                           detail::tuple_has_v<attrs_t, schema::deny_unknown_fields>)) {
            using base_config_t = config::config_of<S>;
            using struct_config_t = detail::annotated_struct_config_t<base_config_t, attrs_t>;
            return detail::serialize_reflectable<struct_config_t, E>(s, value);
        }
        // Default: serialize the underlying value
        else {
            return serialize(s, value);
        }
    } else if constexpr(std::is_enum_v<V>) {
        using underlying_t = std::underlying_type_t<V>;
        if constexpr(std::is_signed_v<underlying_t>) {
            return s.serialize_int(static_cast<std::int64_t>(static_cast<underlying_t>(v)));
        } else {
            return s.serialize_uint(static_cast<std::uint64_t>(static_cast<underlying_t>(v)));
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
    } else if constexpr(null_like<V>) {
        return s.serialize_null();
    } else if constexpr(is_specialization_of<std::optional, V>) {
        if(v.has_value()) {
            return s.serialize_some(v.value());
        } else {
            return s.serialize_null();
        }
    } else if constexpr(is_specialization_of<std::unique_ptr, V> ||
                        is_specialization_of<std::shared_ptr, V>) {
        if(v) {
            return s.serialize_some(*v);
        }
        return s.serialize_null();
    } else if constexpr(is_specialization_of<std::variant, V>) {
        return detail::visit_variant_alt<E, T>(v, [&](const auto& item) {
            return serde::serialize(s, item);
        });
    } else if constexpr(tuple_like<V>) {
        ET_EXPECTED_TRY_V(auto s_tuple,
                          s.serialize_tuple(std::tuple_size_v<std::remove_cvref_t<V>>));

        std::expected<void, E> element_result;
        auto for_each = [&](const auto& element) -> bool {
            auto result = s_tuple.serialize_element(element);
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

        return s_tuple.end();
    } else if constexpr(std::ranges::input_range<V>) {
        constexpr auto kind = format_kind<V>;
        if constexpr(kind == range_format::sequence || kind == range_format::set) {
            std::optional<std::size_t> len = std::nullopt;
            if constexpr(std::ranges::sized_range<V>) {
                len = static_cast<std::size_t>(std::ranges::size(v));
            }

            ET_EXPECTED_TRY_V(auto s_seq, s.serialize_seq(len));

            for(auto&& e: v) {
                ET_EXPECTED_TRY(s_seq.serialize_element(e));
            }

            return s_seq.end();
        } else if constexpr(kind == range_format::map) {
            std::optional<std::size_t> len = std::nullopt;
            if constexpr(std::ranges::sized_range<V>) {
                len = static_cast<std::size_t>(std::ranges::size(v));
            }

            ET_EXPECTED_TRY_V(auto s_map, s.serialize_map(len));

            for(auto&& [key, value]: v) {
                ET_EXPECTED_TRY(s_map.serialize_entry(key, value));
            }

            return s_map.end();
        } else {
            static_assert(dependent_false<V>, "cannot auto serialize the input range");
        }
    } else if constexpr(refl::reflectable_class<V>) {
        using config_t = config::config_of<S>;
        return detail::serialize_reflectable<config_t, E>(s, v);
    } else {
        static_assert(dependent_false<V>,
                      "cannot auto serialize the value, try to specialize for it");
    }
}

template <deserializer_like D, typename V, typename E>
constexpr auto deserialize(D& d, V& v) -> std::expected<void, E> {
    using Deserde = deserialize_traits<D, V>;

    if constexpr(requires { Deserde::deserialize(d, v); }) {
        return Deserde::deserialize(d, v);
    } else if constexpr(annotated_type<V>) {
        using attrs_t = typename std::remove_cvref_t<V>::attrs;
        auto&& value = annotated_value(v);
        using value_t = std::remove_cvref_t<decltype(value)>;

        // Field-only attrs at value level are errors
        static_assert(!detail::tuple_has_v<attrs_t, schema::skip>,
                      "schema::skip is only valid for struct fields");
        static_assert(!detail::tuple_has_v<attrs_t, schema::flatten>,
                      "schema::flatten is only valid for struct fields");

        // Tagged variant dispatch
        if constexpr(is_specialization_of<std::variant, value_t> &&
                     detail::tuple_any_of_v<attrs_t, is_tagged_attr>) {
            using tag_attr = detail::tuple_find_t<attrs_t, is_tagged_attr>;
            constexpr auto strategy = tagged_strategy_of<tag_attr>;
            if constexpr(strategy == tagged_strategy::external) {
                static_assert(variant_support<D>::externally_tagged,
                              "this deserializer does not support externally tagged variants");
                return detail::deserialize_externally_tagged<E>(d, value, tag_attr{});
            } else if constexpr(strategy == tagged_strategy::internal) {
                static_assert(variant_support<D>::internally_tagged,
                              "this deserializer does not support internally tagged variants");
                return detail::deserialize_internally_tagged<E>(d, value, tag_attr{});
            } else {
                static_assert(variant_support<D>::adjacently_tagged,
                              "this deserializer does not support adjacently tagged variants");
                return detail::deserialize_adjacently_tagged<E>(d, value, tag_attr{});
            }
        }
        // Behavior: with/as/enum_string — delegate to apply_deserialize_behavior
        else if constexpr(detail::tuple_count_of_v<attrs_t, is_behavior_provider> > 0) {
            return *detail::apply_deserialize_behavior<attrs_t, value_t, E>(
                value,
                [&](auto& v) { return deserialize(d, v); },
                [&](auto tag, auto& v) -> std::expected<void, E> {
                    using Adapter = typename decltype(tag)::type;
                    return Adapter::deserialize(d, v);
                });
        }
        // Struct-level schema attrs for annotated structs
        else if constexpr(refl::reflectable_class<value_t> &&
                          (detail::tuple_has_spec_v<attrs_t, schema::rename_all> ||
                           detail::tuple_has_v<attrs_t, schema::deny_unknown_fields>)) {
            using base_config_t = config::config_of<D>;
            using struct_config_t = detail::annotated_struct_config_t<base_config_t, attrs_t>;
            constexpr bool deny_unknown = detail::tuple_has_v<attrs_t, schema::deny_unknown_fields>;
            return detail::deserialize_reflectable<struct_config_t, E, deny_unknown>(d, value);
        }
        // Default: deserialize the underlying value
        else {
            return deserialize(d, value);
        }
    } else if constexpr(std::is_enum_v<V>) {
        using underlying_t = std::underlying_type_t<V>;
        if constexpr(std::is_signed_v<underlying_t>) {
            std::int64_t parsed = 0;
            ET_EXPECTED_TRY(d.deserialize_int(parsed));
            if(!detail::integral_value_in_range<underlying_t>(parsed)) {
                return std::unexpected(E::number_out_of_range);
            }
            v = static_cast<V>(static_cast<underlying_t>(parsed));
            return {};
        } else {
            std::uint64_t parsed = 0;
            ET_EXPECTED_TRY(d.deserialize_uint(parsed));
            if(!detail::integral_value_in_range<underlying_t>(parsed)) {
                return std::unexpected(E::number_out_of_range);
            }
            v = static_cast<V>(static_cast<underlying_t>(parsed));
            return {};
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
    } else if constexpr(null_like<V>) {
        ET_EXPECTED_TRY_V(auto is_none, d.deserialize_none());
        if(is_none) {
            v = V{};
            return {};
        }
        return std::unexpected(E::type_mismatch);
    } else if constexpr(is_specialization_of<std::optional, V>) {
        ET_EXPECTED_TRY_V(auto is_none, d.deserialize_none());

        if(is_none) {
            v.reset();
            return {};
        }

        using value_t = typename V::value_type;
        if(v.has_value()) {
            return deserialize(d, v.value());
        }

        if constexpr(std::default_initializable<value_t>) {
            v.emplace();
            auto status = deserialize(d, v.value());
            if(!status) {
                v.reset();
                return std::unexpected(status.error());
            }
            return {};
        } else {
            static_assert(dependent_false<V>,
                          "cannot auto deserialize optional<T> without default-constructible T");
        }
    } else if constexpr(is_specialization_of<std::unique_ptr, V>) {
        ET_EXPECTED_TRY_V(auto is_none, d.deserialize_none());
        if(is_none) {
            v.reset();
            return {};
        }

        using value_t = typename V::element_type;
        static_assert(std::default_initializable<value_t>,
                      "cannot auto deserialize unique_ptr<T> without default-constructible T");
        static_assert(std::same_as<typename V::deleter_type, std::default_delete<value_t>>,
                      "cannot auto deserialize unique_ptr<T, D> with custom deleter");

        auto value = std::make_unique<value_t>();
        ET_EXPECTED_TRY(deserialize(d, *value));

        v = std::move(value);
        return {};
    } else if constexpr(is_specialization_of<std::shared_ptr, V>) {
        ET_EXPECTED_TRY_V(auto is_none, d.deserialize_none());
        if(is_none) {
            v.reset();
            return {};
        }

        using value_t = typename V::element_type;
        static_assert(std::default_initializable<value_t>,
                      "cannot auto deserialize shared_ptr<T> without default-constructible T");

        auto value = std::make_shared<value_t>();
        ET_EXPECTED_TRY(deserialize(d, *value));

        v = std::move(value);
        return {};
    } else if constexpr(is_specialization_of<std::variant, V>) {
        if constexpr(requires { d.peek_type_hint(); d.consume_variant_source(); }) {
            return []<typename... Ts>(D& d, std::variant<Ts...>& v)
                -> std::expected<void, E>
            {
                using config_t = config::config_of<D>;
                ET_EXPECTED_TRY_V(auto hint, d.peek_type_hint());
                ET_EXPECTED_TRY_V(auto source, d.consume_variant_source());

                using source_t = std::remove_cvref_t<decltype(source)>;
                auto hint_bits = static_cast<std::uint8_t>(hint);

                // Object matching: use compile-time schema to select alternative
                if constexpr(requires { D::extract_object_keys(source); }) {
                    if(hint_bits & static_cast<std::uint8_t>(serde::type_hint::object)) {
                        auto keys_result = D::extract_object_keys(source);
                        if(keys_result) {
                            auto idx = schema::match_variant_by_keys<config_t, Ts...>(
                                *keys_result);
                            if(idx.has_value()) {
                                return detail::deserialize_variant_at_index<D, source_t, Ts...>(
                                    *idx, std::forward<decltype(source)>(source), v);
                            }
                        }
                    }
                }

                // Primitive/kind-based matching: collect all candidate indices, sorted
                constexpr auto kind_map = schema::build_kind_to_index_map<Ts...>();
                constexpr std::uint8_t bit_for_kind[] = {
                    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                };

                // Collect candidate indices from all matching kinds
                std::array<std::size_t, sizeof...(Ts)> candidates{};
                std::size_t n_candidates = 0;
                for(std::size_t ki = 0; ki < 8; ki++) {
                    if(hint_bits & bit_for_kind[ki]) {
                        for(std::size_t ci = 0; ci < kind_map.count[ki]; ci++) {
                            auto idx = kind_map.indices[ki][ci];
                            bool dup = false;
                            for(std::size_t j = 0; j < n_candidates; j++) {
                                if(candidates[j] == idx) { dup = true; break; }
                            }
                            if(!dup) {
                                candidates[n_candidates++] = idx;
                            }
                        }
                    }
                }

                // Sort by variant index (ascending = first-declared wins)
                for(std::size_t i = 1; i < n_candidates; i++) {
                    for(std::size_t j = i; j > 0 && candidates[j] < candidates[j-1]; j--) {
                        std::swap(candidates[j], candidates[j-1]);
                    }
                }

                // Try each candidate; first successful deserialize wins
                for(std::size_t ci = 0; ci < n_candidates; ci++) {
                    source_t src_copy = source;
                    auto result = detail::deserialize_variant_at_index<D, source_t, Ts...>(
                        candidates[ci], std::move(src_copy), v);
                    if(result.has_value()) {
                        return result;
                    }
                    if(ci + 1 == n_candidates) {
                        return result;
                    }
                }
                return std::unexpected(E::type_mismatch);
            }(d, v);
        } else {
            static_assert(dependent_false<V>,
                "variant deserialization requires either deserialize_traits "
                "specialization or a self-describing backend "
                "(peek_type_hint + consume_variant_source)");
        }
    } else if constexpr(tuple_like<V>) {
        ET_EXPECTED_TRY_V(auto d_tuple,
                          d.deserialize_tuple(std::tuple_size_v<std::remove_cvref_t<V>>));

        std::expected<void, E> element_result;
        auto read_element = [&](auto& element) -> bool {
            auto result = d_tuple.deserialize_element(element);
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

        return d_tuple.end();
    } else if constexpr(std::ranges::input_range<V>) {
        constexpr auto kind = format_kind<V>;
        if constexpr(kind == range_format::sequence || kind == range_format::set) {
            ET_EXPECTED_TRY_V(auto d_seq, d.deserialize_seq(std::nullopt));

            if constexpr(requires { v.clear(); }) {
                v.clear();
            }

            using element_t = std::ranges::range_value_t<V>;
            static_assert(
                std::default_initializable<element_t>,
                "auto deserialization for ranges requires default-constructible elements");
            static_assert(eventide::detail::sequence_insertable<V, element_t>,
                          "cannot auto deserialize range: container does not support insertion");

            while(true) {
                ET_EXPECTED_TRY_V(auto has_next, d_seq.has_next());
                if(!has_next) {
                    break;
                }

                element_t element{};
                ET_EXPECTED_TRY(d_seq.deserialize_element(element));

                eventide::detail::append_sequence_element(v, std::move(element));
            }

            return d_seq.end();
        } else if constexpr(kind == range_format::map) {
            using key_t = typename V::key_type;
            using mapped_t = typename V::mapped_type;

            ET_EXPECTED_TRY_V(auto d_map, d.deserialize_map(std::nullopt));

            if constexpr(requires { v.clear(); }) {
                v.clear();
            }

            static_assert(
                serde::spelling::parseable_map_key<key_t>,
                "auto map deserialization requires key_type parseable from JSON object keys");
            static_assert(std::default_initializable<mapped_t>,
                          "auto map deserialization requires default-constructible mapped_type");
            static_assert(eventide::detail::map_insertable<V, key_t, mapped_t>,
                          "cannot auto deserialize map: container does not support map insertion");

            while(true) {
                ET_EXPECTED_TRY_V(auto key, d_map.next_key());
                if(!key.has_value()) {
                    break;
                }

                auto parsed_key = serde::spelling::parse_map_key<key_t>(*key);
                if(!parsed_key) {
                    if constexpr(requires { d_map.invalid_key(*key); }) {
                        ET_EXPECTED_TRY(d_map.invalid_key(*key));
                        continue;
                    } else {
                        static_assert(
                            dependent_false<key_t>,
                            "key parse failed and deserializer does not provide invalid_key");
                    }
                }

                mapped_t mapped{};
                ET_EXPECTED_TRY(d_map.deserialize_value(mapped));

                eventide::detail::insert_map_entry(v, std::move(*parsed_key), std::move(mapped));
            }

            return d_map.end();
        } else {
            static_assert(dependent_false<V>, "cannot auto deserialize the input range");
        }
    } else if constexpr(refl::reflectable_class<V>) {
        using config_t = config::config_of<D>;
        return detail::deserialize_reflectable<config_t, E, false>(d, v);
    } else {
        static_assert(dependent_false<V>,
                      "cannot auto deserialize the value, try to specialize for it");
    }
}

}  // namespace eventide::serde
