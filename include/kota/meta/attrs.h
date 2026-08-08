#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "spec.h"
#include "struct.h"
#include "kota/support/fixed_string.h"
#include "kota/support/tuple_traits.h"
#include "kota/support/type_traits.h"

namespace kota::meta {

/// A hint attribute is transparent to the core serde framework.
/// Backends query hints by their own tag type and interpret them freely.
template <typename BackendTag, typename... KV>
struct hint {
    using backend_tag = BackendTag;
    using params = std::tuple<KV...>;
};

/// True for any hint<...> specialization.
template <typename T>
constexpr bool is_hint_attr_v = is_specialization_of<hint, T>;

namespace detail {

template <typename BackendTag, typename T>
constexpr bool hint_matches_v = false;

template <typename BackendTag, typename... KV>
constexpr bool hint_matches_v<BackendTag, hint<BackendTag, KV...>> = true;

template <typename BackendTag, typename... Attrs>
struct find_hint_impl {
    using type = void;
};

template <typename BackendTag, typename First, typename... Rest>
struct find_hint_impl<BackendTag, First, Rest...> {
    using type = std::conditional_t<hint_matches_v<BackendTag, First>,
                                    First,
                                    typename find_hint_impl<BackendTag, Rest...>::type>;
};

}  // namespace detail

/// Get the hint for a specific backend from an attrs tuple.
/// Returns void if no matching hint exists.
template <typename BackendTag, typename AttrsTuple>
struct get_hint;

template <typename BackendTag, typename... Attrs>
struct get_hint<BackendTag, std::tuple<Attrs...>> : detail::find_hint_impl<BackendTag, Attrs...> {};

template <typename BackendTag, typename AttrsTuple>
using get_hint_t = typename get_hint<BackendTag, AttrsTuple>::type;

template <typename T>
concept annotated_type = requires {
    typename std::remove_cvref_t<T>::annotated_type;
    typename std::remove_cvref_t<T>::attrs;
};

namespace attrs {

// Field-level

/// The value attributes of one annotated field. Tag is the struct generated
/// by KOTATSU_ANNOTATE (or hand-written) whose `spec` static member is a
/// meta::spec_result; only the short tag name appears in template arguments.
template <typename Tag>
struct spec {
    constexpr static const field_spec& value = Tag::spec.value;
};

// Struct-level

/// Apply a rename policy to all fields of a struct.
template <typename Policy>
struct rename_all {
    using policy = Policy;
};

/// Reject unknown fields during deserialization.
struct deny_unknown_fields {};

/// Unified tagged variant representation.
/// - tagged<>             = externally tagged  (key is tag name, value is content)
/// - tagged<Tag>          = internally tagged  (tag field embedded in struct)
/// - tagged<Tag, Content> = adjacently tagged  (separate tag and content fields)
///
/// Use ::names<...> to provide custom tag names for each alternative.
/// Without ::names, defaults to meta::type_name<Alt>() for each alternative.
template <fixed_string... FieldNames>
struct tagged {
    static_assert(sizeof...(FieldNames) <= 2, "tagged: 0=external, 1=internal, 2=adjacent");

    constexpr static auto field_names =
        std::array<std::string_view, sizeof...(FieldNames)>{FieldNames...};
    constexpr static bool has_custom_names = false;

    template <fixed_string... Names>
    struct names {
        constexpr static auto field_names =
            std::array<std::string_view, sizeof...(FieldNames)>{FieldNames...};
        constexpr static auto tag_names = std::array<std::string_view, sizeof...(Names)>{Names...};
        constexpr static bool has_custom_names = true;
    };
};

/// Semantic aliases for backward compatibility.
using externally_tagged = tagged<>;

template <fixed_string Tag>
using internally_tagged = tagged<Tag>;

template <fixed_string Tag, fixed_string Content>
using adjacently_tagged = tagged<Tag, Content>;

}  // namespace attrs

template <typename T>
struct is_spec_attr {
    constexpr static bool value = false;
};

template <typename Tag>
struct is_spec_attr<attrs::spec<Tag>> {
    constexpr static bool value = true;
};

inline constexpr field_spec empty_field_spec{};

/// The value spec inside an attrs tuple; the empty spec when absent.
template <typename AttrsTuple>
inline constexpr const field_spec& spec_of = [] -> const field_spec& {
    if constexpr(tuple_any_of_v<AttrsTuple, is_spec_attr>) {
        return tuple_find_t<AttrsTuple, is_spec_attr>::value;
    } else {
        return empty_field_spec;
    }
}();

/// The value spec attached to a (possibly annotated) field type.
template <typename T>
inline constexpr const field_spec& field_spec_of = [] -> const field_spec& {
    if constexpr(annotated_type<T>) {
        return spec_of<typename T::attrs>;
    } else {
        return empty_field_spec;
    }
}();

/// Unified predicate for all tagged attrs (tagged<...> and tagged<...>::names<...>).
template <typename T>
struct is_tagged_attr {
    constexpr static bool value = requires {
        { T::field_names } -> std::convertible_to<std::span<const std::string_view>>;
        { T::has_custom_names } -> std::same_as<const bool&>;
    };
};

/// Strategy dispatch based on field_names count.
enum class tagged_strategy { external = 0, internal = 1, adjacent = 2 };

template <typename TagAttr>
constexpr tagged_strategy tagged_strategy_of =
    static_cast<tagged_strategy>(TagAttr::field_names.size());

/// Resolve tag names for variant alternatives.
/// With ::names, uses the user-provided names (static_assert on count match).
/// Without ::names, uses meta::type_name<Alt>() for each alternative.
template <typename TagAttr, typename... Ts>
constexpr auto resolve_tag_names() {
    if constexpr(TagAttr::has_custom_names) {
        static_assert(TagAttr::tag_names.size() == sizeof...(Ts),
                      "tagged: number of custom names must match variant alternatives");
        return TagAttr::tag_names;
    } else {
        return std::array<std::string_view, sizeof...(Ts)>{type_name<Ts>()...};
    }
}

namespace behavior {

template <typename Policy>
struct enum_string {
    using policy = Policy;
};

template <typename Pred>
struct skip_if {
    using predicate = Pred;
};

/// Adapter-based serialization: the adapter fully controls value (de)serialization.
/// Protocol: Adapter::serialize(S&, const T&) and/or Adapter::deserialize(D&, T&)
template <typename Adapter>
struct with {
    using adapter = Adapter;
};

/// Type conversion: convert to Target type before serializing via default path.
template <typename Target>
struct as {
    using target = Target;
};

}  // namespace behavior

template <typename Pred, typename Value>
constexpr bool evaluate_skip_predicate(const Value& value, bool is_serialize) {
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
            "behavior::skip_if predicate must return bool and accept (const Value&, bool) or (const Value&)");
        return false;
    }
}

namespace pred {

struct optional_none {
    template <typename T>
    constexpr bool operator()(const std::optional<T>& value, bool is_serialize) const {
        return is_serialize && !value.has_value();
    }
};

struct empty {
    template <typename T>
    constexpr bool operator()(const T& value, bool is_serialize) const {
        if constexpr(requires { value.empty(); }) {
            return is_serialize && value.empty();
        } else {
            return false;
        }
    }
};

struct default_value {
    template <typename T>
    constexpr bool operator()(const T& value, bool is_serialize) const {
        if constexpr(requires {
                         T{};
                         value == T{};
                     }) {
            return is_serialize && static_cast<bool>(value == T{});
        } else {
            return false;
        }
    }
};

}  // namespace pred

/// Evaluate a built-in skip_when condition against a field value.
template <skip_when When, typename Value>
constexpr bool evaluate_skip_when(const Value& value, bool is_serialize) {
    static_assert(When != skip_when::never);
    if constexpr(When == skip_when::none) {
        return pred::optional_none{}(value, is_serialize);
    } else if constexpr(When == skip_when::empty) {
        return pred::empty{}(value, is_serialize);
    } else {
        return pred::default_value{}(value, is_serialize);
    }
}

/// True for the closed set of behavior attributes.
template <typename T>
constexpr bool is_behavior_attr_v =
    is_specialization_of<behavior::enum_string, T> || is_specialization_of<behavior::skip_if, T> ||
    is_specialization_of<behavior::with, T> || is_specialization_of<behavior::as, T>;

/// True for behavior providers (with/as/enum_string) — at most one per field.
template <typename T>
struct is_behavior_provider {
    constexpr static bool value = is_specialization_of<behavior::with, T> ||
                                  is_specialization_of<behavior::as, T> ||
                                  is_specialization_of<behavior::enum_string, T>;
};

namespace detail {

template <typename AttrsTuple>
constexpr bool validate_attrs() {
    static_assert(tuple_count_of_v<AttrsTuple, is_behavior_provider> <= 1,
                  "At most one behavior provider (with/as/enum_string) allowed per field");
    static_assert(tuple_count_of_v<AttrsTuple, is_spec_attr> <= 1,
                  "At most one annotation spec allowed per field");
    return true;
}

}  // namespace detail

}  // namespace kota::meta
