#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "config.h"
#include "context.h"
#include "kota/support/ranges.h"
#include "kota/support/type_list.h"
#include "kota/meta/annotation.h"
#include "kota/meta/attrs.h"
#include "kota/meta/enum.h"
#include "kota/meta/schema.h"
#include "kota/meta/struct.h"
#include "kota/meta/type_info.h"
#include "kota/meta/type_kind.h"

namespace kota::codec {

/// User extension point for custom deserialization. Specialize to override default dispatch.
template <typename Vis, typename T, typename Config = default_config<>, typename = void>
struct deserialize_visit {};

template <typename Config, typename Vis, typename T>
bool decode_value(Vis& vis, T& out);

template <typename Config, typename Vis, typename T>
bool decode_struct_fields(Vis& vis, T& out);

template <typename Config, typename TagAttr, typename Vis, typename Var>
bool decode_variant(Vis& vis, Var& var);

namespace detail {

/// True when the visitor uses data-driven (push-style) decoding.
template <typename Vis>
concept data_driven = requires { requires Vis::data_driven; };

/// Ensure an optional/pointer is allocated before writing into it.
template <typename T>
void ensure_allocated(T& out) {
    if constexpr(is_optional_v<T>) {
        if(!out.has_value()) {
            out.emplace();
        }
    } else if constexpr(is_specialization_of<std::unique_ptr, T>) {
        if(!out) {
            out = std::make_unique<typename T::element_type>();
        }
    } else if constexpr(is_specialization_of<std::shared_ptr, T>) {
        if(!out) {
            out = std::make_shared<typename T::element_type>();
        }
    }
}

/// Construct the I-th alternative of a variant and decode into it.
template <typename Config, typename Vis, typename... Ts>
bool construct_and_visit(Vis& vis, std::variant<Ts...>& out, std::size_t index) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
        bool ok = false;
        bool found = ((Is == index ? (out.template emplace<Is>(),
                                      ok = decode_value<Config>(vis, std::get<Is>(out)),
                                      true)
                                   : false) ||
                      ...);
        if(!found) {
            return scoped_context<typename Vis::error_type>::fail(
                rich_error("invalid variant index " + std::to_string(index)));
        }
        return ok;
    }(std::index_sequence_for<Ts...>{});
}

/// Assign a tuple element by runtime index (data-driven tuple decoding).
template <typename Config, typename Vis, typename Tuple>
bool assign_tuple_element(Vis& vis, Tuple& out, std::size_t idx) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
        bool ok = false;
        [[maybe_unused]] bool found =
            ((Is == idx ? (ok = decode_value<Config>(vis, std::get<Is>(out)), true) : false) ||
             ...);
        return ok;
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/// True when the visitor supports peeking the source data kind without consuming.
template <typename Vis>
concept has_peek_kind = requires(Vis& v) {
    { v.peek_kind() } -> std::same_as<meta::type_kind>;
};

/// True when the visitor supports speculative read with automatic rollback on failure.
template <typename Vis>
concept has_try_read = requires(Vis& v) {
    {
        v.try_read([](Vis&) -> bool { return true; })
    } -> std::same_as<bool>;
};

/// Sentinel type: no tag attribute (untagged variant).
struct no_tag {};

/// True when the visitor provides native variant support (bincode/fbs).
template <typename Vis>
concept has_native_variant =
    requires(Vis& v) { v.visit_variant([](std::size_t, auto&) -> bool { return true; }); };

template <typename... Ts>
void emplace_variant_by_index(std::variant<Ts...>& var, std::size_t idx) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((Is == idx ? void(var.template emplace<Is>()) : void()), ...);
    }(std::index_sequence_for<Ts...>{});
}

template <std::size_t N>
std::size_t find_tag_index(std::string_view name, const std::array<std::string_view, N>& names) {
    for(std::size_t i = 0; i < N; ++i) {
        if(names[i] == name)
            return i;
    }
    return N;
}

/// Core behavior-attribute dispatch for a single struct field.
/// Shared by decode_field_value (data-driven) and decode_one_field (schema-driven).
template <typename Config, std::size_t I, typename Vis, typename T>
bool decode_field_inner(Vis& vis, T& out) {
    using schema = meta::virtual_schema<T, Config>;
    using slots = typename schema::slots;
    using slot_t = type_list_element_t<I, slots>;
    using raw_t = std::remove_cv_t<typename slot_t::raw_type>;
    using attrs_t = typename slot_t::attrs;

    constexpr std::size_t offset = schema::fields[I].offset;
    auto* base = reinterpret_cast<std::byte*>(std::addressof(out));
    auto& field_ref = *reinterpret_cast<raw_t*>(base + offset);

    if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::with>) {
        using adapter = typename tuple_find_spec_t<attrs_t, meta::behavior::with>::adapter;
        if constexpr(requires {
                         adapter::from_wire(std::declval<typename adapter::wire_type>());
                     }) {
            using wire_t = typename adapter::wire_type;
            wire_t wire{};
            KOTA_CODEC_TRY(decode_value<Config>(vis, wire));
            field_ref = adapter::from_wire(std::move(wire));
            return true;
        } else if constexpr(requires(decltype(vis)& v) { adapter::deserialize(v, field_ref); }) {
            return adapter::deserialize(vis, field_ref);
        } else {
            return decode_value<Config>(vis, field_ref);
        }
    } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::as>) {
        using target = typename tuple_find_spec_t<attrs_t, meta::behavior::as>::target;
        target converted{};
        KOTA_CODEC_TRY(decode_value<Config>(vis, converted));
        field_ref = raw_t(std::move(converted));
        return true;
    } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::enum_string>) {
        using policy = typename tuple_find_spec_t<attrs_t, meta::behavior::enum_string>::policy;
        static_assert(std::is_enum_v<raw_t>, "behavior::enum_string requires an enum type");
        std::string name_str;
        KOTA_CODEC_TRY(vis.visit_str(name_str));
        auto renamed = policy{}(false, name_str);
        auto val = meta::enum_value<raw_t>(renamed);
        if(val) {
            field_ref = *val;
            return true;
        }
        return scoped_context<typename Vis::error_type>::fail(
            rich_error(std::string("unknown enum value '") + name_str + "'"));
    } else if constexpr(tuple_any_of_v<attrs_t, meta::is_tagged_attr>) {
        static_assert(meta::kind_of<raw_t>() == meta::type_kind::variant,
                      "tagged attribute requires a variant type");
        if constexpr(!is_human_readable<Config, Vis>()) {
            return decode_value<Config>(vis, field_ref);
        } else {
            using tag_attr = tuple_find_t<attrs_t, meta::is_tagged_attr>;
            return decode_variant<Config, tag_attr>(vis, field_ref);
        }
    } else {
        return decode_value<Config>(vis, field_ref);
    }
}

/// Decode a field's value applying behavior transforms, without visit_field wrapping.
/// Used by match_field (data-driven path) where the field reader is already provided.
template <typename Config, std::size_t I, typename Vis, typename T>
bool decode_field_value(Vis& vis, T& out) {
    using schema = meta::virtual_schema<T, Config>;
    using slots = typename schema::slots;
    using slot_t = type_list_element_t<I, slots>;
    using raw_t = std::remove_cv_t<typename slot_t::raw_type>;
    using attrs_t = typename slot_t::attrs;

    std::string_view wire_name = schema::fields[I].name;

    if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::skip_if>) {
        constexpr std::size_t offset = schema::fields[I].offset;
        auto* base = reinterpret_cast<std::byte*>(std::addressof(out));
        auto& field_ref = *reinterpret_cast<raw_t*>(base + offset);
        using pred = typename tuple_find_spec_t<attrs_t, meta::behavior::skip_if>::predicate;
        if(meta::evaluate_skip_predicate<pred>(field_ref, false)) {
            if constexpr(requires { vis.visit_skip(); }) {
                return vis.visit_skip();
            } else {
                return true;
            }
        }
    }

    bool ok = decode_field_inner<Config, I>(vis, out);

    if constexpr(Config::detailed_error) {
        if(!ok) {
            if(auto* e = scoped_context<typename Vis::error_type>::try_current())
                e->prepend_field(wire_name);
        }
    }
    return ok;
}

/// Data-driven field matching: look up key in virtual_schema fields and decode the matching field.
/// If field_mask is provided, sets the bit corresponding to the matched field index.
template <typename Config, typename T, typename Vis>
bool match_field(std::string_view key, Vis& reader, T& out, std::uint64_t* field_mask = nullptr) {
    using schema = meta::virtual_schema<T, Config>;
    using slots = typename schema::slots;
    constexpr std::size_t N = type_list_size_v<slots>;
    static_assert(N <= 64, "struct field count exceeds field_mask capacity (max 64 fields)");

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
        bool matched = false;
        bool result = true;
        (void)((!matched && ([&] {
                   constexpr auto& fi = schema::fields[Is];
                   bool found = fi.name == key;
                   if(!found) {
                       for(auto alias: fi.aliases) {
                           if(alias == key) {
                               found = true;
                               break;
                           }
                       }
                   }
                   if(found) {
                       matched = true;
                       if(field_mask)
                           *field_mask |= (1ULL << Is);
                       result = decode_field_value<Config, Is>(reader, out);
                   }
                   return found;
               }())) ||
               ...);

        if(!matched) {
            if constexpr(Config::deny_unknown_fields || schema::deny_unknown) {
                return scoped_context<typename Vis::error_type>::fail(
                    rich_error::unknown_field(key));
            } else {
                return reader.visit_skip();
            }
        }
        return result;
    }(std::make_index_sequence<N>{});
}

/// After data-driven struct decode, validate that all required fields were present.
/// A field is required if it is not optional/pointer/null, has no skip_if behavior,
/// and is not annotated with default_value.
template <typename Config, typename T, typename Vis>
bool check_required_fields(std::uint64_t field_mask) {
    using schema = meta::virtual_schema<T, Config>;
    using slots = typename schema::slots;
    constexpr std::size_t N = type_list_size_v<slots>;
    static_assert(N <= 64, "struct field count exceeds field_mask capacity (max 64 fields)");

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
        return (([&] {
                    if(field_mask & (1ULL << Is))
                        return true;
                    using slot_t = type_list_element_t<Is, slots>;
                    using raw_t = std::remove_cv_t<typename slot_t::raw_type>;
                    using attrs_t = typename slot_t::attrs;

                    [[maybe_unused]] constexpr auto kind = meta::kind_of<raw_t>();
                    if constexpr(kind == meta::type_kind::optional ||
                                 kind == meta::type_kind::pointer ||
                                 kind == meta::type_kind::null) {
                        return true;
                    } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::skip_if>) {
                        return true;
                    } else if constexpr(tuple_has_v<attrs_t, meta::attrs::default_value>) {
                        return true;
                    } else if constexpr(meta::annotated_type<raw_t>) {
                        using inner_attrs = typename raw_t::attrs;
                        if constexpr(tuple_has_v<inner_attrs, meta::attrs::default_value>) {
                            return true;
                        } else {
                            using inner_t = meta::annotated_underlying_t<raw_t>;
                            constexpr auto inner_kind = meta::kind_of<inner_t>();
                            if constexpr(inner_kind == meta::type_kind::optional ||
                                         inner_kind == meta::type_kind::pointer ||
                                         inner_kind == meta::type_kind::null) {
                                return true;
                            } else {
                                return scoped_context<typename Vis::error_type>::fail(
                                    rich_error::missing_field(schema::fields[Is].name));
                            }
                        }
                    } else {
                        return scoped_context<typename Vis::error_type>::fail(
                            rich_error::missing_field(schema::fields[Is].name));
                    }
                }()) &&
                ...);
    }(std::make_index_sequence<N>{});
}

/// External tagged: { "TagName": value }
template <typename Config, typename TagAttr, typename Vis, typename... Ts>
bool decode_externally_tagged(Vis& vis, std::variant<Ts...>& var) {
    constexpr auto names = meta::resolve_tag_names<TagAttr, Ts...>();
    bool found = false;
    bool result = vis.visit_struct([&](std::string_view key, auto& fv) -> bool {
        if(found) {
            return scoped_context<typename Vis::error_type>::fail(
                rich_error("externally tagged variant: expected exactly one field"));
        }
        found = true;
        auto idx = find_tag_index(key, names);
        if(idx >= sizeof...(Ts)) {
            return scoped_context<typename Vis::error_type>::fail(
                rich_error(std::string("unknown variant tag '") + std::string(key) + "'"));
        }
        return construct_and_visit<Config>(fv, var, idx);
    });
    if(result && !found) {
        return scoped_context<typename Vis::error_type>::fail(
            rich_error("externally tagged variant: expected exactly one field"));
    }
    return result;
}

/// Internal tagged: { "tag": "TagName", ...fields... }
/// Three paths: try_read pre-lookup, streaming (tag first), schema-driven (struct_reader).
template <typename Config, typename TagAttr, typename Vis, typename... Ts>
bool decode_internally_tagged(Vis& vis, std::variant<Ts...>& var) {
    constexpr std::string_view tag_key = TagAttr::field_names[0];
    constexpr auto names = meta::resolve_tag_names<TagAttr, Ts...>();
    constexpr std::size_t npos = sizeof...(Ts);

    std::size_t idx = npos;

    // Pre-lookup: try_read + data-driven scan for tag field
    if constexpr(has_try_read<Vis> && data_driven<Vis>) {
        vis.try_read([&](auto& fork) -> bool {
            fork.visit_struct([&](std::string_view key, auto& fv) -> bool {
                if(key == tag_key) {
                    std::string name;
                    fv.visit_str(name);
                    idx = find_tag_index(std::string_view(name), names);
                    return false;
                }
                return true;
            });
            return false;
        });
    }

    if constexpr(data_driven<Vis>) {
        // Data-driven main pass
        if(idx != npos)
            emplace_variant_by_index(var, idx);

        std::uint64_t field_mask = 0;
        bool result = vis.visit_struct([&](std::string_view key, auto& fv) -> bool {
            if(key == tag_key) {
                if(idx != npos)
                    return fv.visit_skip();
                // Streaming: resolve tag inline (must be first)
                std::string tag_value;
                KOTA_CODEC_TRY(fv.visit_str(tag_value));
                idx = find_tag_index(tag_value, names);
                if(idx >= npos) {
                    return scoped_context<typename Vis::error_type>::fail(
                        rich_error(std::string("unknown variant tag '") + tag_value + "'"));
                }
                emplace_variant_by_index(var, idx);
                return true;
            }
            if(idx == npos) {
                return scoped_context<typename Vis::error_type>::fail(
                    rich_error("internally tagged variant: tag must appear before data fields"));
            }
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
                bool r = true;
                ((Is == idx
                      ? void(r = match_field<Config,
                                             std::variant_alternative_t<Is, std::variant<Ts...>>>(
                                 key,
                                 fv,
                                 std::get<Is>(var),
                                 &field_mask))
                      : void()),
                 ...);
                return r;
            }(std::index_sequence_for<Ts...>{});
        });

        if(result && idx != npos) {
            result = [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
                bool ok = true;
                ((Is == idx ? void(ok = check_required_fields<
                                       Config,
                                       std::variant_alternative_t<Is, std::variant<Ts...>>,
                                       Vis>(field_mask))
                            : void()),
                 ...);
                return ok;
            }(std::index_sequence_for<Ts...>{});
        }
        return result;
    } else {
        // Schema-driven: struct_reader with find_field / visit_field
        return vis.visit_struct(var, [&](auto& sv) -> bool {
            std::string tag_value;
            if constexpr(requires {
                             sv.find_field(std::string_view{}, [](auto&) -> bool { return true; });
                         }) {
                KOTA_CODEC_TRY(sv.find_field(tag_key, [&](auto& tv) -> bool {
                    return tv.visit_str(tag_value);
                }));
            } else {
                KOTA_CODEC_TRY(sv.visit_field(std::size_t(0), tag_key, [&](auto& tv) -> bool {
                    return tv.visit_str(tag_value);
                }));
            }

            idx = find_tag_index(tag_value, names);
            if(idx >= npos) {
                return scoped_context<typename Vis::error_type>::fail(
                    rich_error(std::string("unknown variant tag '") + tag_value + "'"));
            }

            return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
                bool ok = false;
                ((Is == idx ? (var.template emplace<Is>(),
                               ok = decode_struct_fields<Config>(sv, std::get<Is>(var)),
                               true)
                            : false) ||
                 ...);
                return ok;
            }(std::index_sequence_for<Ts...>{});
        });
    }
}

/// Adjacent tagged: { "t": "TagName", "c": value }
template <typename Config, typename TagAttr, typename Vis, typename... Ts>
bool decode_adjacently_tagged(Vis& vis, std::variant<Ts...>& var) {
    constexpr std::string_view tag_key = TagAttr::field_names[0];
    constexpr std::string_view content_key = TagAttr::field_names[1];
    constexpr auto names = meta::resolve_tag_names<TagAttr, Ts...>();
    constexpr std::size_t npos = sizeof...(Ts);

    std::size_t idx = npos;

    // Pre-lookup tag via try_read
    if constexpr(has_try_read<Vis> && data_driven<Vis>) {
        vis.try_read([&](auto& fork) -> bool {
            fork.visit_struct([&](std::string_view key, auto& fv) -> bool {
                if(key == tag_key) {
                    std::string name;
                    fv.visit_str(name);
                    idx = find_tag_index(std::string_view(name), names);
                    return false;
                }
                return true;
            });
            return false;
        });
    }

    if constexpr(data_driven<Vis>) {
        std::size_t tag_count = 0;
        std::size_t content_count = 0;

        bool result = vis.visit_struct([&](std::string_view key, auto& fv) -> bool {
            if(key == tag_key) {
                ++tag_count;
                if(idx != npos)
                    return fv.visit_skip();
                std::string tag_value;
                KOTA_CODEC_TRY(fv.visit_str(tag_value));
                idx = find_tag_index(tag_value, names);
                if(idx >= npos) {
                    return scoped_context<typename Vis::error_type>::fail(
                        rich_error(std::string("unknown variant tag '") + tag_value + "'"));
                }
                emplace_variant_by_index(var, idx);
                return true;
            }
            if(key == content_key) {
                ++content_count;
                if(idx == npos) {
                    return scoped_context<typename Vis::error_type>::fail(
                        rich_error("adjacently tagged variant: content before tag"));
                }
                if(content_count > 1)
                    return fv.visit_skip();
                return construct_and_visit<Config>(fv, var, idx);
            }
            return fv.visit_skip();
        });

        if(!result)
            return false;
        if(idx == npos) {
            return scoped_context<typename Vis::error_type>::fail(
                rich_error("adjacently tagged variant: missing tag field"));
        }
        if(content_count == 0) {
            return scoped_context<typename Vis::error_type>::fail(
                rich_error("adjacently tagged variant: missing content field"));
        }
        if(tag_count > 1) {
            return scoped_context<typename Vis::error_type>::fail(
                rich_error("adjacently tagged variant: duplicate tag field"));
        }
        if(content_count > 1) {
            return scoped_context<typename Vis::error_type>::fail(
                rich_error("adjacently tagged variant: duplicate content field"));
        }
        return true;
    } else {
        return vis.visit_struct(var, [&](auto& sv) -> bool {
            std::string tag_value;
            KOTA_CODEC_TRY(sv.visit_field(std::size_t(0), tag_key, [&](auto& tv) -> bool {
                return tv.visit_str(tag_value);
            }));

            idx = find_tag_index(tag_value, names);
            if(idx >= npos) {
                return scoped_context<typename Vis::error_type>::fail(
                    rich_error(std::string("unknown variant tag '") + tag_value + "'"));
            }

            return sv.visit_field(std::size_t(1), content_key, [&](auto& cv) -> bool {
                return construct_and_visit<Config>(cv, var, idx);
            });
        });
    }
}

template <typename T>
constexpr bool kind_compatible(meta::type_kind src) {
    using enum meta::type_kind;
    constexpr auto target = meta::kind_of<T>();

    if(src == unknown)
        return true;
    if constexpr(target == boolean)
        return src == boolean;
    else if constexpr(meta::int_like<T> || meta::uint_like<T>)
        return src == int64 || src == uint64;
    else if constexpr(target == float32 || target == float64)
        return src == float64;
    else if constexpr(target == character || meta::str_like<T>)
        return src == string;
    else if constexpr(target == null)
        return src == null;
    else if constexpr(target == optional || target == pointer)
        return true;
    else if constexpr(target == structure)
        return src == structure;
    else if constexpr(target == array || target == set)
        return src == array;
    else if constexpr(target == map)
        return src == structure;
    else if constexpr(target == variant)
        return true;
    else if constexpr(target == enumeration)
        return src == string || src == int64 || src == uint64;
    else
        return true;
}

template <typename Config, typename Vis, typename... Ts>
bool decode_untagged_variant(Vis& vis, std::variant<Ts...>& out) {
    if constexpr(has_try_read<Vis>) {
        if constexpr(has_peek_kind<Vis>) {
            auto src_kind = vis.peek_kind();
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
                [[maybe_unused]] constexpr std::size_t last = sizeof...(Ts) - 1;
                return (([&] {
                            using alt_t = std::variant_alternative_t<Is, std::variant<Ts...>>;
                            if constexpr(Is != last) {
                                if(!kind_compatible<alt_t>(src_kind))
                                    return false;
                                return vis.try_read([&](auto& fork) -> bool {
                                    return construct_and_visit<Config>(fork, out, Is);
                                });
                            } else {
                                return construct_and_visit<Config>(vis, out, Is);
                            }
                        }()) ||
                        ...);
            }(std::index_sequence_for<Ts...>{});
        } else {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
                [[maybe_unused]] constexpr std::size_t last = sizeof...(Ts) - 1;
                return (([&] {
                            if constexpr(Is == last) {
                                return construct_and_visit<Config>(vis, out, Is);
                            } else {
                                return vis.try_read([&](auto& fork) -> bool {
                                    return construct_and_visit<Config>(fork, out, Is);
                                });
                            }
                        }()) ||
                        ...);
            }(std::index_sequence_for<Ts...>{});
        }
    } else {
        static_assert(
            has_try_read<Vis>,
            "untagged variant decode requires a visitor with try_read or peek_kind support");
        return false;
    }
}

/// Decode a single struct field, applying behavior transforms if present.
/// Mirrors encode_one_field: wraps the decode in vis.visit_field(idx, wire_name, ...).
template <typename Config, std::size_t I, typename Vis, typename T>
bool decode_one_field(Vis& vis, T& out) {
    using schema = meta::virtual_schema<T, Config>;
    using slots = typename schema::slots;
    using slot_t = type_list_element_t<I, slots>;
    using raw_t = std::remove_cv_t<typename slot_t::raw_type>;
    using attrs_t = typename slot_t::attrs;

    constexpr auto idx = std::integral_constant<std::size_t, I>{};
    std::string_view wire_name = schema::fields[I].name;

    if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::skip_if>) {
        constexpr std::size_t offset = schema::fields[I].offset;
        auto* base = reinterpret_cast<std::byte*>(std::addressof(out));
        auto& field_ref = *reinterpret_cast<raw_t*>(base + offset);
        using pred = typename tuple_find_spec_t<attrs_t, meta::behavior::skip_if>::predicate;
        if(meta::evaluate_skip_predicate<pred>(field_ref, false)) {
            raw_t discard{};
            return vis.visit_field(idx, wire_name, [&](auto& fv) -> bool {
                return decode_value<Config>(fv, discard);
            });
        }
    }

    bool ok = vis.visit_field(idx, wire_name, [&](auto& fv) -> bool {
        return decode_field_inner<Config, I>(fv, out);
    });

    if constexpr(Config::detailed_error) {
        if(!ok) {
            if(auto* e = scoped_context<typename Vis::error_type>::try_current())
                e->prepend_field(wire_name);
        }
    }
    return ok;
}

}  // namespace detail

/// Unified variant decode: dispatches to native, tagged, or untagged path.
template <typename Config, typename TagAttr, typename Vis, typename Var>
bool decode_variant(Vis& vis, Var& var) {
    return [&]<typename... Ts>(std::variant<Ts...>&) -> bool {
        if constexpr(detail::has_native_variant<Vis>) {
            return vis.visit_variant([&](std::size_t index, auto& pv) -> bool {
                return detail::construct_and_visit<Config>(pv, var, index);
            });
        } else if constexpr(!std::is_same_v<TagAttr, detail::no_tag>) {
            if constexpr(!is_human_readable<Config, Vis>()) {
                return detail::decode_untagged_variant<Config>(vis, var);
            } else {
                constexpr auto strategy = meta::tagged_strategy_of<TagAttr>;
                if constexpr(strategy == meta::tagged_strategy::external) {
                    return detail::decode_externally_tagged<Config, TagAttr>(vis, var);
                } else if constexpr(strategy == meta::tagged_strategy::internal) {
                    return detail::decode_internally_tagged<Config, TagAttr>(vis, var);
                } else {
                    static_assert(strategy == meta::tagged_strategy::adjacent);
                    return detail::decode_adjacently_tagged<Config, TagAttr>(vis, var);
                }
            }
        } else {
            return detail::decode_untagged_variant<Config>(vis, var);
        }
    }(var);
}

template <typename Config, typename Vis, typename T>
bool decode_value(Vis& vis, T& out) {
    using V = std::remove_const_t<T>;

    if constexpr(requires(Vis& v, V& val) { deserialize_visit<Vis, V, Config>::visit(v, val); }) {
        return deserialize_visit<Vis, V, Config>::visit(vis, out);
    } else if constexpr(meta::annotated_type<V>) {
        using attrs_t = typename V::attrs;
        auto&& inner = meta::annotated_value(out);
        using inner_t = std::remove_cvref_t<decltype(inner)>;

        if constexpr(is_specialization_of<std::variant, inner_t> &&
                     tuple_any_of_v<attrs_t, meta::is_tagged_attr>) {
            if constexpr(!is_human_readable<Config, Vis>()) {
                return decode_value<Config>(vis, inner);
            } else {
                using tag_attr = tuple_find_t<attrs_t, meta::is_tagged_attr>;
                return decode_variant<Config, tag_attr>(vis, inner);
            }
        } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::enum_string>) {
            using policy = typename tuple_find_spec_t<attrs_t, meta::behavior::enum_string>::policy;
            static_assert(std::is_enum_v<inner_t>, "behavior::enum_string requires an enum type");
            std::string name_str;
            KOTA_CODEC_TRY(vis.visit_str(name_str));
            auto renamed = policy{}(false, name_str);
            auto val = meta::enum_value<inner_t>(renamed);
            if(val) {
                inner = *val;
                return true;
            }
            return scoped_context<typename Vis::error_type>::fail(
                rich_error(std::string("unknown enum value '") + name_str + "'"));
        } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::with>) {
            using adapter = typename tuple_find_spec_t<attrs_t, meta::behavior::with>::adapter;
            if constexpr(requires {
                             adapter::from_wire(std::declval<typename adapter::wire_type>());
                         }) {
                using wire_t = typename adapter::wire_type;
                wire_t wire{};
                KOTA_CODEC_TRY(decode_value<Config>(vis, wire));
                inner = adapter::from_wire(std::move(wire));
                return true;
            } else if constexpr(requires { adapter::deserialize(vis, inner); }) {
                return adapter::deserialize(vis, inner);
            } else {
                return decode_value<Config>(vis, inner);
            }
        } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::as>) {
            using target = typename tuple_find_spec_t<attrs_t, meta::behavior::as>::target;
            target converted{};
            KOTA_CODEC_TRY(decode_value<Config>(vis, converted));
            inner = inner_t(std::move(converted));
            return true;
        } else if constexpr(meta::reflectable_class<inner_t> &&
                            (tuple_has_spec_v<attrs_t, meta::attrs::rename_all> ||
                             tuple_has_v<attrs_t, meta::attrs::deny_unknown_fields>)) {
            using merged_config = detail::annotated_config<Config, attrs_t>;
            return decode_value<merged_config>(vis, inner);
        } else {
            return decode_value<Config>(vis, inner);
        }
    } else {
        constexpr auto kind = meta::kind_of<V>();
        using enum meta::type_kind;

        if constexpr(kind == boolean) {
            return vis.visit_bool(out);
        } else if constexpr(meta::int_like<V>) {
            return vis.visit_int(out);
        } else if constexpr(meta::uint_like<V>) {
            return vis.visit_uint(out);
        } else if constexpr(kind == float32 || kind == float64) {
            return vis.visit_float(out);
        } else if constexpr(meta::str_like<V>) {
            return vis.visit_str(out);
        } else if constexpr(kind == character) {
            return vis.visit_char(out);
        } else if constexpr(kind == bytes) {
            return vis.visit_bytes(out);
        } else if constexpr(kind == null) {
            return vis.visit_null();
        } else if constexpr(kind == optional || kind == pointer) {
            if constexpr(requires { vis.visit_option(out, [](auto&) -> bool { return true; }); }) {
                return vis.visit_option(out, [&](auto& sv) -> bool {
                    detail::ensure_allocated(out);
                    return decode_value<Config>(sv, *out);
                });
            } else {
                if(vis.peek_null()) {
                    out = V{};
                    return vis.visit_null();
                }
                detail::ensure_allocated(out);
                return decode_value<Config>(vis, *out);
            }
        } else if constexpr(kind == enumeration) {
            if constexpr(Config::enum_repr == enum_repr::String) {
                std::string name_str;
                KOTA_CODEC_TRY(vis.visit_str(name_str));
                auto renamed = apply_enum_rename<Config>(false, name_str);
                auto val = meta::enum_value<V>(renamed);
                if(val) {
                    out = *val;
                    return true;
                }
                return scoped_context<typename Vis::error_type>::fail(
                    rich_error(std::string("unknown enum value '") + name_str + "'"));
            } else if constexpr(requires { vis.visit_enum(out); }) {
                return vis.visit_enum(out);
            } else {
                using U = std::underlying_type_t<V>;
                U underlying{};
                bool ok;
                if constexpr(std::is_signed_v<U>) {
                    ok = vis.visit_int(underlying);
                } else {
                    ok = vis.visit_uint(underlying);
                }
                if(ok) {
                    out = static_cast<V>(underlying);
                }
                return ok;
            }
        } else if constexpr(kind == structure) {
            if constexpr(detail::data_driven<Vis>) {
                std::uint64_t field_mask = 0;
                bool result = vis.visit_struct([&](std::string_view key, auto& fv) -> bool {
                    return detail::match_field<Config, V>(key, fv, out, &field_mask);
                });
                if(result) {
                    result = detail::check_required_fields<Config, V, Vis>(field_mask);
                }
                return result;
            } else {
                return vis.visit_struct(out, [&](auto& sv) -> bool {
                    return decode_struct_fields<Config>(sv, out);
                });
            }
        } else if constexpr(kind == array || kind == set) {
            using element_t = std::ranges::range_value_t<V>;
            if constexpr(detail::data_driven<Vis>) {
                if constexpr(requires { out.clear(); }) {
                    out.clear();
                }
                std::size_t idx = 0;
                return vis.visit_seq([&](auto& ev) -> bool {
                    element_t item{};
                    bool ok = decode_value<Config>(ev, item);
                    if(!ok) {
                        if constexpr(Config::detailed_error) {
                            if(auto* e = scoped_context<typename Vis::error_type>::try_current())
                                e->prepend_index(idx);
                        }
                        return false;
                    }
                    kota::detail::append_sequence_element(out, std::move(item));
                    ++idx;
                    return true;
                });
            } else {
                return vis.visit_seq(out, [&](auto& sv) -> bool {
                    if constexpr(requires { out.clear(); }) {
                        out.clear();
                    }
                    std::size_t idx = 0;
                    while(sv.has_element()) {
                        element_t item{};
                        bool ok = sv.visit_element(
                            [&](auto& ev) -> bool { return decode_value<Config>(ev, item); });
                        if(!ok) {
                            if constexpr(Config::detailed_error) {
                                if(auto* e =
                                       scoped_context<typename Vis::error_type>::try_current())
                                    e->prepend_index(idx);
                            }
                            return false;
                        }
                        kota::detail::append_sequence_element(out, std::move(item));
                        ++idx;
                    }
                    return true;
                });
            }
        } else if constexpr(kind == tuple) {
            if constexpr(detail::data_driven<Vis>) {
                constexpr std::size_t expected = std::tuple_size_v<V>;
                std::size_t idx = 0;
                bool seq_ok = vis.visit_tuple([&]([[maybe_unused]] auto& ev) -> bool {
                    if constexpr(expected == 0) {
                        return scoped_context<typename Vis::error_type>::fail(
                            rich_error("too many elements for tuple (expected 0)"));
                    } else {
                        if(idx >= expected) {
                            return scoped_context<typename Vis::error_type>::fail(
                                rich_error("too many elements for tuple (expected " +
                                           std::to_string(expected) + ")"));
                        }
                        bool ok = detail::assign_tuple_element<Config>(ev, out, idx);
                        if(!ok) {
                            if constexpr(Config::detailed_error) {
                                if(auto* e =
                                       scoped_context<typename Vis::error_type>::try_current())
                                    e->prepend_index(idx);
                            }
                            return false;
                        }
                        ++idx;
                        return true;
                    }
                });
                if(!seq_ok)
                    return false;
                if(idx != expected) {
                    return scoped_context<typename Vis::error_type>::fail(rich_error(
                        "too few elements for tuple (expected " + std::to_string(expected) +
                        ", got " + std::to_string(idx) + ")"));
                }
                return true;
            } else {
                return vis.visit_tuple(out, [&](auto& sv) -> bool {
                    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                        return ([&] {
                            bool ok = sv.visit_element([&](auto& ev) -> bool {
                                return decode_value<Config>(ev, std::get<Is>(out));
                            });
                            if constexpr(Config::detailed_error) {
                                if(!ok) {
                                    if(auto* e =
                                           scoped_context<typename Vis::error_type>::try_current())
                                        e->prepend_index(Is);
                                }
                            }
                            return ok;
                        }() && ...);
                    }(std::make_index_sequence<std::tuple_size_v<V>>{});
                });
            }
        } else if constexpr(kind == map) {
            using kv_t = std::ranges::range_value_t<V>;
            using key_t = std::remove_const_t<typename kv_t::first_type>;
            using mapped_t = typename kv_t::second_type;
            if constexpr(detail::data_driven<Vis>) {
                if constexpr(requires { out.clear(); }) {
                    out.clear();
                }
                std::size_t idx = 0;
                return vis.visit_map([&](auto& kv, auto& vv) -> bool {
                    key_t key{};
                    bool ok = decode_value<Config>(kv, key);
                    if(!ok) {
                        if constexpr(Config::detailed_error) {
                            if(auto* e = scoped_context<typename Vis::error_type>::try_current())
                                e->prepend_index(idx);
                        }
                        return false;
                    }
                    mapped_t val{};
                    ok = decode_value<Config>(vv, val);
                    if(!ok) {
                        if constexpr(Config::detailed_error) {
                            if(auto* e = scoped_context<typename Vis::error_type>::try_current())
                                e->prepend_index(idx);
                        }
                        return false;
                    }
                    kota::detail::insert_map_entry(out, std::move(key), std::move(val));
                    ++idx;
                    return true;
                });
            } else {
                return vis.visit_map(out, [&](auto& sv) -> bool {
                    if constexpr(requires { out.clear(); }) {
                        out.clear();
                    }
                    std::size_t idx = 0;
                    while(sv.has_entry()) {
                        key_t key{};
                        mapped_t val{};
                        bool ok = sv.visit_entry(
                            [&](auto& kv) -> bool { return decode_value<Config>(kv, key); },
                            [&](auto& vv) -> bool { return decode_value<Config>(vv, val); });
                        if(!ok) {
                            if constexpr(Config::detailed_error) {
                                if(auto* e =
                                       scoped_context<typename Vis::error_type>::try_current())
                                    e->prepend_index(idx);
                            }
                            return false;
                        }
                        kota::detail::insert_map_entry(out, std::move(key), std::move(val));
                        ++idx;
                    }
                    return true;
                });
            }
        } else if constexpr(kind == variant) {
            return decode_variant<Config, detail::no_tag>(vis, out);
        } else {
            static_assert(
                dependent_false<V>,
                "cannot deserialize this type; specialize deserialize_visit to add support");
            return false;
        }
    }
}

template <typename Config, typename Vis, typename T>
bool decode_struct_fields(Vis& vis, T& out) {
    using schema = meta::virtual_schema<T, Config>;
    using slots = typename schema::slots;
    constexpr std::size_t N = type_list_size_v<slots>;

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return (detail::decode_one_field<Config, Is>(vis, out) && ...);
    }(std::make_index_sequence<N>{});
}

}  // namespace kota::codec
