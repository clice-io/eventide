#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "kota/support/config.h"

namespace kota::meta {

/// Built-in conditions for omitting a field during serialization. Custom
/// predicates use the `skip_if = type<Pred>` component instead.
enum class skip_when : std::uint8_t {
    never,
    /// std::optional field without a value.
    none,
    /// Container whose .empty() returns true.
    empty,
    /// Value that compares equal to a default-constructed one.
    default_value,
};

/// Fixed-capacity list of alias names for one field.
struct name_list {
    constexpr static std::size_t capacity = 8;

    std::array<std::string_view, capacity> storage = {};
    std::size_t count = 0;

    constexpr std::span<const std::string_view> names() const {
        return {storage.data(), count};
    }
};

/// The value part of a field annotation. One non-template type, built by
/// KOTATSU_ANNOTATE at compile time; downstream code reads it through
/// field_spec_of so mangled names never contain the string payloads. Designed
/// to be attachable verbatim as a C++26 [[=...]] annotation object later.
struct field_spec {
    constexpr static std::uint32_t no_idx = 0xFFFF'FFFF;

    /// Wire name replacing the reflected field name; empty means unset.
    std::string_view rename = {};
    /// Documentation exported by schema backends; transparent on the wire.
    std::string_view description = {};
    /// Extra accepted names during deserialization.
    name_list alias = {};
    /// Field ordinal for index-addressed formats (flatbuffers/protobuf).
    std::uint32_t idx = no_idx;
    /// Built-in serialization-skip condition.
    skip_when skip_if = skip_when::never;
    /// Exclude the field from (de)serialization entirely.
    bool skip = false;
    /// Inline the fields of a nested struct into the parent.
    bool flatten = false;
    /// Allow the field to be absent during deserialization (keeps its
    /// default-constructed value). Equivalent to Rust's #[serde(default)].
    bool defaulted = false;
};

/// Carries a type into the annotation DSL: `as = type<Target>`.
template <typename T>
struct type_tag {};

namespace dsl {

/// Which spec aspect a component sets; used to reject duplicates.
enum class aspect : std::uint8_t {
    rename,
    description,
    alias,
    idx,
    skip,
    flatten,
    defaulted,
    skip_if,
    as,
    with,
    enum_string,
};

template <aspect A, auto Member, typename V>
struct value_component {
    V value;

    constexpr static aspect kind = A;

    constexpr void apply(field_spec& spec) const {
        spec.*Member = value;
    }
};

struct alias_component {
    name_list value;

    constexpr static aspect kind = aspect::alias;

    constexpr void apply(field_spec& spec) const {
        spec.alias = value;
    }
};

/// A component that carries a type instead of a value (as/with/enum_string
/// and custom skip_if predicates). annotation.h maps it to the equivalent
/// behavior attr; the spec value is untouched.
template <aspect A, typename T>
struct type_component {
    using target = T;

    constexpr static aspect kind = A;

    constexpr void apply(field_spec&) const {}
};

template <typename T>
constexpr bool is_type_component_v = false;

template <aspect A, typename T>
constexpr bool is_type_component_v<type_component<A, T>> = true;

template <typename T>
concept annotation_component = requires(const T& component, field_spec& spec) {
    { T::kind } -> std::convertible_to<aspect>;
    component.apply(spec);
};

template <aspect A, auto Member, typename V>
struct value_proxy {
    constexpr auto operator=(V value) const {
        return value_component<A, Member, V>{value};
    }
};

struct alias_proxy {
    constexpr auto operator=(std::initializer_list<std::string_view> names) const {
        if(names.size() > name_list::capacity) {
            KOTA_THROW("annotation: too many aliases");
        }
        alias_component component{};
        for(auto name: names) {
            component.value.storage[component.value.count++] = name;
        }
        return component;
    }
};

/// `skip_if` accepts both a built-in skip_when value and a custom predicate
/// type; the two forms conflict through the shared aspect.
struct skip_if_proxy {
    constexpr auto operator=(skip_when when) const {
        return value_component<aspect::skip_if, &field_spec::skip_if, skip_when>{when};
    }

    template <typename Pred>
    constexpr auto operator=(type_tag<Pred>) const {
        return type_component<aspect::skip_if, Pred>{};
    }
};

template <aspect A>
struct type_proxy {
    template <typename T>
    constexpr auto operator=(type_tag<T>) const {
        return type_component<A, T>{};
    }
};

constexpr inline value_proxy<aspect::rename, &field_spec::rename, std::string_view> rename{};
constexpr inline value_proxy<aspect::description, &field_spec::description, std::string_view>
    description{};
constexpr inline value_proxy<aspect::idx, &field_spec::idx, std::uint32_t> idx{};
constexpr inline value_proxy<aspect::skip, &field_spec::skip, bool> skip{};
constexpr inline value_proxy<aspect::flatten, &field_spec::flatten, bool> flatten{};
constexpr inline value_proxy<aspect::defaulted, &field_spec::defaulted, bool> defaulted{};
constexpr inline alias_proxy alias{};
constexpr inline skip_if_proxy skip_if{};
constexpr inline type_proxy<aspect::as> as{};
constexpr inline type_proxy<aspect::with> with{};
constexpr inline type_proxy<aspect::enum_string> enum_string{};

template <typename T>
constexpr inline type_tag<T> type{};

using meta::skip_when;

}  // namespace dsl

/// What KOTATSU_ANNOTATE stores in its tag: the folded value spec, plus the
/// type components preserved in the result type.
template <typename... TypeComponents>
struct spec_result {
    field_spec value;

    using extras = std::tuple<TypeComponents...>;
};

namespace detail {

template <typename... Cs>
consteval bool component_kinds_unique() {
    std::array kinds = {Cs::kind...};
    for(std::size_t i = 0; i < kinds.size(); ++i) {
        for(std::size_t j = i + 1; j < kinds.size(); ++j) {
            if(kinds[i] == kinds[j]) {
                return false;
            }
        }
    }
    return true;
}

constexpr void validate_spec(const field_spec& spec) {
    if(spec.skip && spec.flatten) {
        KOTA_THROW("annotation: skip and flatten conflict");
    }
    auto aliases = spec.alias.names();
    for(std::size_t i = 0; i < aliases.size(); ++i) {
        if(aliases[i] == spec.rename) {
            KOTA_THROW("annotation: alias duplicates the rename name");
        }
        for(std::size_t j = i + 1; j < aliases.size(); ++j) {
            if(aliases[i] == aliases[j]) {
                KOTA_THROW("annotation: duplicate alias name");
            }
        }
    }
}

}  // namespace detail

template <typename... Cs>
constexpr auto make_spec(const Cs&... components) {
    static_assert((dsl::annotation_component<Cs> && ...),
                  "annotation entries must be assignments, e.g. skip = true");
    static_assert(detail::component_kinds_unique<Cs...>(),
                  "annotation: the same attribute appears twice");

    field_spec spec{};
    (components.apply(spec), ...);
    detail::validate_spec(spec);

    using extras_t = decltype(std::tuple_cat(
        std::declval<
            std::conditional_t<dsl::is_type_component_v<Cs>, std::tuple<Cs>, std::tuple<>>>()...));
    return []<typename... Ts>(const field_spec& value, std::type_identity<std::tuple<Ts...>>) {
        return spec_result<Ts...>{value};
    }(spec, std::type_identity<extras_t>{});
}

}  // namespace kota::meta
