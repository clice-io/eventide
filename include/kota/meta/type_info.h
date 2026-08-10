#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

#include "annotation.h"
#include "enum.h"
#include "repr.h"
#include "struct.h"
#include "type_kind.h"
#include "kota/support/naming.h"
#include "kota/support/ranges.h"
#include "kota/support/tuple_traits.h"

namespace kota::meta {

struct default_config {};

struct field_info;

struct type_info {
    type_kind kind;
    std::string_view type_name;

    constexpr bool is_integer() const {
        return kind >= type_kind::int8 && kind <= type_kind::uint64;
    }

    constexpr bool is_signed_integer() const {
        return kind >= type_kind::int8 && kind <= type_kind::int64;
    }

    constexpr bool is_unsigned_integer() const {
        return kind >= type_kind::uint8 && kind <= type_kind::uint64;
    }

    constexpr bool is_floating() const {
        return kind == type_kind::float32 || kind == type_kind::float64;
    }

    constexpr bool is_numeric() const {
        return is_integer() || is_floating();
    }

    constexpr bool is_scalar() const {
        return kind <= type_kind::enumeration;
    }
};

using type_info_fn = const type_info& (*)();

struct array_type_info : type_info {
    type_info_fn element;
};

struct map_type_info : type_info {
    type_info_fn key;
    type_info_fn value;
};

struct enum_type_info : type_info {
    std::span<const std::string_view> member_names;
    const void* member_values;
    type_kind underlying_kind;
};

struct tuple_type_info : type_info {
    std::span<const type_info_fn> elements;
};

struct variant_type_info : type_info {
    std::span<const type_info_fn> alternatives;
    tag_mode tagging = tag_mode::none;
    std::string_view tag_field;
    std::string_view content_field;
    std::span<const std::string_view> alt_names;
};

struct optional_type_info : type_info {
    type_info_fn inner;
};

struct field_info {
    std::string_view name;
    std::span<const std::string_view> aliases;
    std::size_t offset;
    std::size_t physical_index;
    type_info_fn type;

    bool has_default;
    bool has_skip_if;
    bool has_behavior;

    /// Field ordinal for index-addressed formats; field_spec::no_idx when
    /// absent. Metadata only for now: no backend consumes it yet.
    std::uint32_t idx = field_spec::no_idx;

    /// Documentation text from the annotation, empty when absent.
    std::string_view description = {};
};

struct struct_type_info : type_info {
    bool deny_unknown;
    bool is_trivial_layout;
    std::span<const field_info> fields;
};

template <typename T, typename Config = default_config>
constexpr const type_info& type_info_of();

namespace detail {

template <typename Policy>
constexpr std::string apply_rename_cx(std::string_view input) {
    using namespace naming::rename_policy;
    if constexpr(std::is_same_v<Policy, identity>) {
        return std::string(input);
    } else if constexpr(std::is_same_v<Policy, lower_snake>) {
        return naming::normalize_to_lower_snake(input);
    } else if constexpr(std::is_same_v<Policy, lower_camel>) {
        return naming::snake_to_camel(input, false);
    } else if constexpr(std::is_same_v<Policy, upper_camel>) {
        return naming::snake_to_camel(input, true);
    } else if constexpr(std::is_same_v<Policy, upper_snake>) {
        return naming::snake_to_upper(input);
    } else {
        static_assert(sizeof(Policy) == 0, "Unknown rename policy");
    }
}

template <typename T, std::size_t I, typename Policy>
struct wire_name_static {
    constexpr static std::size_t len = apply_rename_cx<Policy>(meta::field_name<I, T>()).size();

    constexpr static auto storage = [] {
        auto renamed = apply_rename_cx<Policy>(meta::field_name<I, T>());
        std::array<char, len> arr{};
        for(std::size_t i = 0; i < len; ++i)
            arr[i] = renamed[i];
        return arr;
    }();

    constexpr static std::string_view value{storage.data(), storage.size()};
};

template <typename Config>
struct effective_field_rename {
    using type = naming::rename_policy::identity;
};

template <typename Config>
    requires requires { typename Config::field_rename; }
struct effective_field_rename<Config> {
    using type = typename Config::field_rename;
};

template <typename Config>
using effective_field_rename_t = typename effective_field_rename<Config>::type;

template <typename T, std::size_t I, typename Config>
constexpr std::string_view resolve_wire_name() {
    using policy = effective_field_rename_t<Config>;
    constexpr const field_spec& spec = field_spec_of<meta::field_type<T, I>>;
    if constexpr(!spec.rename.empty()) {
        return spec.rename;
    } else if constexpr(std::is_same_v<policy, naming::rename_policy::identity>) {
        return meta::field_name<I, T>();
    } else {
        return wire_name_static<T, I, policy>::value;
    }
}

/// True for a spec attr whose values matter beyond schema building (decode
/// default handling, encode skip conditions); name-only specs are dropped
/// from encode slots so identically-shaped fields share one slot type.
template <typename Attr>
constexpr bool is_runtime_spec_attr_v = false;

template <typename Tag>
constexpr bool is_runtime_spec_attr_v<attrs::spec<Tag>> =
    attrs::spec<Tag>::value.defaulted || attrs::spec<Tag>::value.skip_if != skip_when::never;

/// A struct spec whose values matter at encode/decode dispatch: the tagged
/// variant paths read the tagging mode and names from the slot attrs, and a
/// rename_all/deny_unknown spec on a field merges into the config there.
template <typename Attr>
constexpr bool is_runtime_struct_spec_attr_v = false;

template <typename Tag>
constexpr bool is_runtime_struct_spec_attr_v<attrs::struct_spec<Tag>> =
    attrs::struct_spec<Tag>::value.tagging != tag_mode::none ||
    attrs::struct_spec<Tag>::value.rename_all != naming::casing::identity ||
    attrs::struct_spec<Tag>::value.deny_unknown_fields;

template <typename Tuple>
struct filter_runtime_attrs;

template <>
struct filter_runtime_attrs<std::tuple<>> {
    using type = std::tuple<>;
};

template <typename First, typename... Rest>
struct filter_runtime_attrs<std::tuple<First, Rest...>> {
    using tail = typename filter_runtime_attrs<std::tuple<Rest...>>::type;
    constexpr static bool keep = is_behavior_attr_v<First> ||
                                 is_runtime_struct_spec_attr_v<First> ||
                                 is_runtime_spec_attr_v<First>;
    using type = std::conditional_t<keep,
                                    decltype(std::tuple_cat(std::declval<std::tuple<First>>(),
                                                            std::declval<tail>())),
                                    tail>;
};

template <typename Tuple>
using filter_runtime_attrs_t = typename filter_runtime_attrs<Tuple>::type;

/// Tag for a struct spec rekeyed by its structural values; see type_attrs_t.
template <naming::casing RenameAll, bool DenyUnknown>
struct struct_spec_value_tag {
    constexpr static struct_spec spec = {.rename_all = RenameAll,
                                         .deny_unknown_fields = DenyUnknown};
};

/// The attrs that shape the type-level schema (variant tagging, struct-wide
/// rename, unknown-field policy). Field-local attrs (rename/description/
/// default/...) are dropped so annotated and bare uses of the same type share
/// one type_info instance — and thus one $defs entry in schema output.
/// KOTATSU_ANNOTATE also mints a fresh tag per use, making textually identical
/// annotations distinct types, so an untagged struct spec is rekeyed by its
/// structural values and equivalent annotations share one instance as well.
/// Tagged variant specs keep their own tag (the string payloads are not
/// structural); schema backends emit variants inline rather than as shared
/// defs, so distinct instances are harmless there.
template <typename AttrsTuple>
constexpr auto type_attrs_impl() {
    constexpr const struct_spec& spec = struct_spec_of<AttrsTuple>;
    if constexpr(spec.tagging != tag_mode::none) {
        return std::type_identity<std::tuple<tuple_find_t<AttrsTuple, is_struct_spec_attr>>>{};
    } else if constexpr(spec.rename_all != naming::casing::identity || spec.deny_unknown_fields) {
        return std::type_identity<std::tuple<attrs::struct_spec<
            struct_spec_value_tag<spec.rename_all, spec.deny_unknown_fields>>>>{};
    } else {
        return std::type_identity<std::tuple<>>{};
    }
}

template <typename AttrsTuple>
using type_attrs_t = typename decltype(type_attrs_impl<AttrsTuple>())::type;

template <typename RawType, typename AttrsTuple>
constexpr auto resolve_wire_type_impl() {
    if constexpr(tuple_has_spec_v<AttrsTuple, behavior::as>) {
        return std::type_identity<typename tuple_find_spec_t<AttrsTuple, behavior::as>::target>{};
    } else if constexpr(tuple_has_spec_v<AttrsTuple, behavior::enum_string>) {
        return std::type_identity<std::string_view>{};
    } else if constexpr(tuple_has_spec_v<AttrsTuple, behavior::with>) {
        using adapter = typename tuple_find_spec_t<AttrsTuple, behavior::with>::adapter;
        static_assert(
            requires { typename adapter::type; },
            "behavior::with adapter must declare its wire shape via `using type = ...`");
        return std::type_identity<typename adapter::type>{};
    } else if constexpr(has_repr<RawType>) {
        static_assert(
            requires { typename repr<RawType>::type; },
            "meta::repr<T> must declare its wire shape via `using type = ...` "
            "(meta::dynamic when only known at runtime)");
        return std::type_identity<typename repr<RawType>::type>{};
    } else {
        return std::type_identity<RawType>{};
    }
}

template <typename RawType, typename AttrsTuple>
using resolve_wire_type_t = typename decltype(resolve_wire_type_impl<RawType, AttrsTuple>())::type;

template <typename T>
struct unwrap_annotated {
    using raw_type = T;
    using attrs = std::tuple<>;
};

template <annotated_type T>
struct unwrap_annotated<T> {
    using raw_type = typename T::annotated_type;
    using attrs = typename T::attrs;
};

template <typename BaseConfig,
          typename AttrsTuple,
          bool HasRenameAll = struct_spec_of<AttrsTuple>.rename_all != naming::casing::identity>
struct struct_schema_config {
    using type = BaseConfig;
};

template <typename BaseConfig, typename AttrsTuple>
struct struct_schema_config<BaseConfig, AttrsTuple, true> {
    struct type : BaseConfig {
        using field_rename = naming::rename_policy_t<struct_spec_of<AttrsTuple>.rename_all>;
    };
};

template <typename BaseConfig, typename AttrsTuple>
using struct_schema_config_t = typename struct_schema_config<BaseConfig, AttrsTuple>::type;

template <typename WireT, typename AttrsT, typename Config, type_kind Kind = kind_of<WireT>()>
struct type_instance_impl;

template <typename T, typename Config = default_config>
struct type_instance :
    type_instance_impl<resolve_wire_type_t<typename unwrap_annotated<std::remove_cv_t<T>>::raw_type,
                                           typename unwrap_annotated<std::remove_cv_t<T>>::attrs>,
                       type_attrs_t<typename unwrap_annotated<std::remove_cv_t<T>>::attrs>,
                       Config> {};

template <typename T, std::size_t I>
constexpr std::size_t single_field_count();

template <typename T>
    requires meta::reflectable_class<T>
constexpr std::size_t effective_field_count() {
    constexpr std::size_t N = meta::field_count<T>();
    if constexpr(N == 0) {
        return 0;
    } else {
        return []<std::size_t... Is>(std::index_sequence<Is...>) constexpr {
            return (single_field_count<T, Is>() + ...);
        }(std::make_index_sequence<N>{});
    }
}

template <typename T, std::size_t I>
constexpr std::size_t single_field_count() {
    using field_t = meta::field_type<T, I>;
    constexpr const field_spec& spec = field_spec_of<field_t>;

    if constexpr(spec.skip) {
        return 0;
    } else if constexpr(spec.flatten) {
        using inner_t = std::remove_cvref_t<typename unwrap_annotated<field_t>::raw_type>;
        static_assert(meta::reflectable_class<inner_t>,
                      "flatten requires the field type to be a reflectable struct");
        return effective_field_count<inner_t>();
    } else {
        return 1;
    }
}

template <typename T, std::size_t I>
struct field_attr_flags {
    constexpr static bool skipped = field_spec_of<meta::field_type<T, I>>.skip;
    constexpr static bool flattened = field_spec_of<meta::field_type<T, I>>.flatten;
};

template <typename T, typename Config>
using built_fields_t = std::array<field_info, effective_field_count<T>()>;

template <typename T, typename Config>
constexpr built_fields_t<T, Config> build_fields(std::size_t base_offset = 0);

template <typename T>
constexpr bool has_deny_unknown_fields() {
    using attrs_t = typename unwrap_annotated<T>::attrs;
    return struct_spec_of<attrs_t>.deny_unknown_fields;
}

template <typename Variant, typename Config, typename AttrsTuple = std::tuple<>>
struct variant_info_node;

template <typename Config, typename AttrsTuple, typename... Ts>
struct variant_info_node<std::variant<Ts...>, Config, AttrsTuple> {
    using variant_t = std::variant<Ts...>;
    constexpr const static struct_spec& spec = struct_spec_of<AttrsTuple>;
    constexpr static bool has_tag = spec.tagging != tag_mode::none;

    constexpr static std::array<type_info_fn, sizeof...(Ts)> alternatives = {
        type_info_of<Ts, Config>...};

    // Backing storage is always sizeof...(Ts) (>=1 since variant must have alternatives);
    // for non-tagged variants the elements stay default-constructed and the consumer
    // span below is explicitly given size 0, so no element is ever read.
    constexpr static auto alt_names = [] {
        if constexpr(has_tag) {
            return resolve_tag_names<tuple_find_t<AttrsTuple, is_struct_spec_attr>, Ts...>();
        } else {
            return std::array<std::string_view, sizeof...(Ts)>{};
        }
    }();

    constexpr inline static variant_type_info value = {
        {type_kind::variant,  meta::type_name<variant_t>()  },
        {alternatives.data(), alternatives.size()           },
        spec.tagging,
        spec.tag,
        spec.content,
        {alt_names.data(),    has_tag ? alt_names.size() : 0},
    };
};

template <typename Tuple,
          typename Config,
          typename Seq = std::make_index_sequence<std::tuple_size_v<Tuple>>>
struct tuple_info_node;

template <typename Tuple, typename Config, std::size_t... Is>
struct tuple_info_node<Tuple, Config, std::index_sequence<Is...>> {
    constexpr inline static std::array<type_info_fn, sizeof...(Is)> elements = {
        type_info_of<std::tuple_element_t<Is, Tuple>, Config>...};

    constexpr inline static tuple_type_info value = {
        {type_kind::tuple, meta::type_name<Tuple>()},
        {elements.data(),  elements.size()         },
    };
};

template <typename WireT, typename AttrsT, typename Config, type_kind Kind>
struct type_instance_impl {
    constexpr inline static type_info value = {
        kind_of<WireT>(),
        meta::type_name<WireT>(),
    };
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::optional> {
    using inner_t = typename WireT::value_type;

    constexpr inline static optional_type_info value = {
        {type_kind::optional, meta::type_name<WireT>()},
        type_info_of<inner_t, Config>,
    };
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::pointer> {
    using inner_t = typename WireT::element_type;

    constexpr inline static optional_type_info value = {
        {type_kind::pointer, meta::type_name<WireT>()},
        type_info_of<inner_t, Config>,
    };
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::variant> {
    constexpr inline static variant_type_info value =
        variant_info_node<WireT, Config, AttrsT>::value;
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::tuple> {
    constexpr inline static tuple_type_info value = tuple_info_node<WireT, Config>::value;
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::map> {
    using kv_t = std::ranges::range_value_t<WireT>;
    using key_t = std::remove_const_t<typename kv_t::first_type>;
    using mapped_t = typename kv_t::second_type;

    constexpr inline static map_type_info value = {
        {type_kind::map, meta::type_name<WireT>()},
        type_info_of<key_t, Config>,
        type_info_of<mapped_t, Config>,
    };
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::set> {
    using element_t = std::ranges::range_value_t<WireT>;

    constexpr inline static array_type_info value = {
        {type_kind::set, meta::type_name<WireT>()},
        type_info_of<element_t, Config>,
    };
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::array> {
    using element_t = std::ranges::range_value_t<WireT>;

    constexpr inline static array_type_info value = {
        {type_kind::array, meta::type_name<WireT>()},
        type_info_of<element_t, Config>,
    };
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::structure> {
    using schema_config = struct_schema_config_t<Config, AttrsT>;
    constexpr static std::size_t count = effective_field_count<WireT>();
    constexpr static bool deny_unknown =
        has_deny_unknown_fields<WireT>() || struct_spec_of<AttrsT>.deny_unknown_fields;
    constexpr static bool is_trivially_copyable = std::is_trivially_copyable_v<WireT>;

    constexpr inline static built_fields_t<WireT, schema_config> fields =
        build_fields<WireT, schema_config>();

    constexpr inline static struct_type_info value = {
        {type_kind::structure, meta::type_name<WireT>()},
        deny_unknown,
        is_trivially_copyable,
        {fields.data(),        count                   },
    };
};

template <typename WireT, typename AttrsT, typename Config>
struct type_instance_impl<WireT, AttrsT, Config, type_kind::enumeration> {
    constexpr static auto& names = meta::reflection<WireT>::member_names;
    constexpr static auto& values = meta::reflection<WireT>::member_values;
    using underlying_t = std::underlying_type_t<WireT>;

    constexpr inline static enum_type_info value = {
        {type_kind::enumeration, meta::type_name<WireT>()},
        {names.data(),           names.size()            },
        static_cast<const void*>(values.data()),
        kind_of<underlying_t>(),
    };
};

template <typename T, typename Config, std::size_t I>
constexpr void fill_field(auto& result, std::size_t& out, std::size_t base_offset);

template <typename T, typename Config, std::size_t I>
constexpr field_info make_field_info(std::size_t base_offset) {
    using field_t = meta::field_type<T, I>;
    using attrs_t = typename unwrap_annotated<field_t>::attrs;
    constexpr const field_spec& spec = field_spec_of<field_t>;

    return field_info{
        .name = resolve_wire_name<T, I, Config>(),
        .aliases = spec.alias.names(),
        .offset = base_offset + meta::field_offset<T>(I),
        .physical_index = I,
        .type = type_info_of<field_t, Config>,
        .has_default = spec.defaulted,
        .has_skip_if =
            spec.skip_if != skip_when::never || tuple_has_spec_v<attrs_t, behavior::skip_if>,
        .has_behavior = tuple_any_of_v<attrs_t, is_behavior_provider>,
        .idx = spec.idx,
        .description = spec.description,
    };
}

template <typename T, typename Config>
constexpr built_fields_t<T, Config> build_fields(std::size_t base_offset) {
    built_fields_t<T, Config> result{};
    std::size_t out = 0;

    constexpr std::size_t N = meta::field_count<T>();
    if constexpr(N > 0) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) constexpr {
            (fill_field<T, Config, Is>(result, out, base_offset), ...);
        }(std::make_index_sequence<N>{});
    }

    return result;
}

template <typename T, typename Config, std::size_t I>
constexpr void fill_field(auto& result, std::size_t& out, std::size_t base_offset) {
    using field_t = meta::field_type<T, I>;
    constexpr const field_spec& spec = field_spec_of<field_t>;

    if constexpr(spec.skip) {
    } else if constexpr(spec.flatten) {
        using inner_t = typename unwrap_annotated<field_t>::raw_type;
        std::size_t inner_offset = base_offset + meta::field_offset<T>(I);
        auto inner = build_fields<inner_t, Config>(inner_offset);
        for(std::size_t i = 0; i < inner.size(); ++i) {
            result[out++] = inner[i];
        }
    } else {
        result[out++] = make_field_info<T, Config, I>(base_offset);
    }
}

}  // namespace detail

template <typename T, typename Config>
constexpr const type_info& type_info_of() {
    return detail::type_instance<T, Config>::value;
}

}  // namespace kota::meta
