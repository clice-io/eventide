#pragma once

#include <expected>
#include <type_traits>
#include <variant>

#include "eventide/common/expected_try.h"
#include "eventide/common/meta.h"
#include "eventide/reflection/struct.h"
#include "eventide/serde/serde/annotation.h"
#include "eventide/serde/serde/attrs/behavior.h"
#include "eventide/serde/serde/attrs/schema.h"
#include "eventide/serde/serde/utils/apply_behavior.h"

namespace eventide::serde::detail {

struct sequential_policy {
    static constexpr bool allow_flatten = true;
    static constexpr bool allow_tagged_variant = true;
    static constexpr bool consume_on_skip_if = true;
};

struct indexed_policy {
    static constexpr bool allow_flatten = false;
    static constexpr bool allow_tagged_variant = false;
    static constexpr bool consume_on_skip_if = false;
};

template <typename Config, typename E, typename Policy, typename Field,
          typename ReadFn, typename WithFn>
constexpr auto deserialize_positional_struct_field(
    Field field,
    ReadFn&& read_fn,
    WithFn&& with_fn
) -> std::expected<void, E> {
    using field_t = typename std::remove_cvref_t<decltype(field)>::type;

    if constexpr(!annotated_type<field_t>) {
        ETD_EXPECTED_TRY(read_fn(field.index(), field.value()));
        return {};
    } else {
        using attrs_t = typename std::remove_cvref_t<field_t>::attrs;
        auto&& value = annotated_value(field.value());
        using value_t = std::remove_cvref_t<decltype(value)>;

        // Schema: skip — field not on wire
        if constexpr(tuple_has_v<attrs_t, schema::skip>) {
            return {};
        }
        // Schema: flatten — inline nested struct fields
        else if constexpr(tuple_has_v<attrs_t, schema::flatten>) {
            if constexpr(Policy::allow_flatten) {
                static_assert(refl::reflectable_class<value_t>,
                              "schema::flatten requires a reflectable class field type");
                std::expected<void, E> nested_status{};
                refl::for_each(value, [&](auto nested_field) {
                    auto status = deserialize_positional_struct_field<Config, E, Policy>(
                        nested_field, read_fn, with_fn);
                    if(!status) {
                        nested_status = std::unexpected(status.error());
                        return false;
                    }
                    return true;
                });
                return nested_status;
            } else {
                static_assert(dependent_false<field_t>,
                              "schema::flatten is not supported by this backend");
                return {};
            }
        } else {
            // Behavior: skip_if — conditionally skip deserialization
            if constexpr(tuple_has_spec_v<attrs_t, behavior::skip_if>) {
                using Pred = typename tuple_find_spec_t<attrs_t, behavior::skip_if>::predicate;
                if(evaluate_skip_predicate<Pred>(value, false)) {
                    if constexpr(Policy::consume_on_skip_if) {
                        using consume_t = std::remove_cvref_t<decltype(field.value())>;
                        static_assert(std::default_initializable<consume_t>,
                                      "consume_on_skip_if requires default-initializable field");
                        consume_t skipped{};
                        ETD_EXPECTED_TRY(read_fn(field.index(), skipped));
                    }
                    return {};
                }
            }

            // Behavior: with/as/enum_string — delegate to apply_deserialize_behavior
            if constexpr(tuple_count_of_v<attrs_t, is_behavior_provider> > 0) {
                auto field_reader = [&](auto& v) { return read_fn(field.index(), v); };
                ETD_EXPECTED_TRY((*apply_deserialize_behavior<attrs_t, value_t, E>(
                    value, field_reader, with_fn)));
                return {};
            }
            // Tagged variant — preserve annotation wrapper
            else if constexpr(is_specialization_of<std::variant, value_t> &&
                              tuple_any_of_v<attrs_t, is_tagged_attr>) {
                if constexpr(Policy::allow_tagged_variant) {
                    ETD_EXPECTED_TRY(read_fn(field.index(), field.value()));
                    return {};
                } else {
                    static_assert(dependent_false<field_t>,
                                  "tagged variant is not supported by this backend");
                    return {};
                }
            }
            // Default — strip annotation
            else {
                ETD_EXPECTED_TRY(read_fn(field.index(), annotated_value(field.value())));
                return {};
            }
        }
    }
}

template <typename Config, typename E, typename Policy, typename T,
          typename ReadFn, typename WithFn>
constexpr auto deserialize_positional_struct(
    T& value,
    ReadFn&& read_fn,
    WithFn&& with_fn
) -> std::expected<void, E> {
    std::expected<void, E> result{};
    refl::for_each(value, [&](auto field) {
        auto status = deserialize_positional_struct_field<Config, E, Policy>(
            field, read_fn, with_fn);
        if(!status) {
            result = std::unexpected(status.error());
            return false;
        }
        return true;
    });
    return result;
}

}  // namespace eventide::serde::detail
