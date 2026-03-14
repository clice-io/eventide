#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "eventide/reflection/struct.h"
#include "eventide/serde/serde/attrs/schema.h"
#include "eventide/serde/serde/config.h"
#include "eventide/serde/serde/schema/descriptor.h"
#include "eventide/serde/serde/schema/kind.h"
#include "eventide/serde/serde/spelling.h"

namespace eventide::serde::schema {

// ─── Key + value kind pair from incoming data ───────────────────────────────

struct incoming_field {
    std::string name;
    type_kind kind;
};

namespace detail {

// ─── Constexpr string buffer for compile-time wire name computation ─────────

struct name_buf {
    char data[64]{};
    std::uint8_t len = 0;

    static constexpr name_buf from(std::string_view s) {
        name_buf b;
        auto n = s.size() < 63 ? s.size() : 63;
        for(std::size_t i = 0; i < n; i++) b.data[i] = s[i];
        b.len = static_cast<std::uint8_t>(n);
        return b;
    }

    constexpr std::string_view view() const { return {data, len}; }
    constexpr void push_back(char c) { data[len++] = c; }
    constexpr char back() const { return data[len - 1]; }
    constexpr bool empty() const { return len == 0; }
};

// ─── Constexpr rename policy implementations ────────────────────────────────

constexpr name_buf normalize_to_lower_snake_cx(std::string_view text) {
    using namespace spelling::detail;

    name_buf out;
    for(std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if(is_ascii_alnum(c)) {
            if(is_ascii_upper(c)) {
                const bool prev_alnum = i > 0 && is_ascii_alnum(text[i - 1]);
                const bool prev_lower_digit =
                    i > 0 && (is_ascii_lower(text[i - 1]) || is_ascii_digit(text[i - 1]));
                const bool next_lower = i + 1 < text.size() && is_ascii_lower(text[i + 1]);
                if(!out.empty() && out.back() != '_' && prev_alnum &&
                   (prev_lower_digit || next_lower)) {
                    out.push_back('_');
                }
                out.push_back(ascii_lower(c));
            } else {
                out.push_back(ascii_lower(c));
            }
        } else if(!out.empty() && out.back() != '_') {
            out.push_back('_');
        }
    }

    std::size_t start = 0;
    while(start < out.len && out.data[start] == '_') start++;
    std::size_t end = out.len;
    while(end > start && out.data[end - 1] == '_') end--;

    if(start > 0 || end < out.len) {
        name_buf trimmed;
        for(std::size_t i = start; i < end; i++) trimmed.push_back(out.data[i]);
        return trimmed;
    }
    return out;
}

constexpr name_buf snake_to_camel_cx(std::string_view text, bool upper_first) {
    using namespace spelling::detail;
    auto snake = normalize_to_lower_snake_cx(text);
    name_buf out;
    bool capitalize_next = upper_first;
    bool seen_output = false;
    for(std::size_t i = 0; i < snake.len; i++) {
        char c = snake.data[i];
        if(c == '_') {
            capitalize_next = true;
            continue;
        }
        if(capitalize_next && is_ascii_alpha(c)) {
            out.push_back(ascii_upper(c));
        } else if(!seen_output) {
            out.push_back(upper_first ? ascii_upper(c) : ascii_lower(c));
        } else {
            out.push_back(c);
        }
        capitalize_next = false;
        seen_output = true;
    }
    return out;
}

constexpr name_buf snake_to_upper_cx(std::string_view text) {
    using namespace spelling::detail;
    auto snake = normalize_to_lower_snake_cx(text);
    for(std::size_t i = 0; i < snake.len; i++) {
        snake.data[i] = ascii_upper(snake.data[i]);
    }
    return snake;
}

template <typename Policy>
constexpr name_buf apply_rename_cx(std::string_view input) {
    if constexpr(std::same_as<Policy, spelling::rename_policy::identity>) {
        return name_buf::from(input);
    } else if constexpr(std::same_as<Policy, spelling::rename_policy::lower_snake>) {
        return normalize_to_lower_snake_cx(input);
    } else if constexpr(std::same_as<Policy, spelling::rename_policy::lower_camel>) {
        return snake_to_camel_cx(input, false);
    } else if constexpr(std::same_as<Policy, spelling::rename_policy::upper_camel>) {
        return snake_to_camel_cx(input, true);
    } else if constexpr(std::same_as<Policy, spelling::rename_policy::upper_snake>) {
        return snake_to_upper_cx(input);
    } else {
        return name_buf::from(input);
    }
}

// ─── Kind compatibility ─────────────────────────────────────────────────────

constexpr bool kind_compatible(type_kind incoming, type_kind schema_kind) {
    if(incoming == type_kind::any || schema_kind == type_kind::any) return true;
    if(incoming == schema_kind) return true;
    if(incoming == type_kind::integer && schema_kind == type_kind::floating) return true;
    if(incoming == type_kind::floating && schema_kind == type_kind::integer) return true;
    return false;
}

// ─── Compile-time field entry ───────────────────────────────────────────────

struct ct_field_entry {
    name_buf wire_name;
    type_kind kind = type_kind::any;
    bool required = false;
};

// ─── Count entries (fields + aliases, flatten-expanded) ─────────────────────

template <typename T>
consteval std::size_t count_match_entries();

template <typename T, std::size_t I>
consteval std::size_t single_match_contribution() {
    if constexpr(field_has_skip<T, I>()) {
        return 0;
    } else if constexpr(field_has_flatten<T, I>()) {
        using field_t = refl::field_type<T, I>;
        using inner_t = unwrapped_t<field_t>;
        static_assert(refl::reflectable_class<inner_t>);
        return count_match_entries<inner_t>();
    } else {
        return 1 + serde::schema::detail::alias_count<T, I>();
    }
}

template <typename T>
consteval std::size_t count_match_entries() {
    if constexpr(!refl::reflectable_class<std::remove_cvref_t<T>>) {
        return 0;
    } else {
        using U = std::remove_cvref_t<T>;
        constexpr auto N = refl::field_count<U>();
        if constexpr(N == 0) {
            return 0;
        } else {
            return []<std::size_t... Is>(std::index_sequence<Is...>) consteval {
                return (single_match_contribution<U, Is>() + ...);
            }(std::make_index_sequence<N>{});
        }
    }
}

// ─── Count required fields (unique fields only, no aliases) ─────────────────

template <typename T>
consteval std::size_t count_required_fields();

template <typename T, std::size_t I>
consteval std::size_t single_required_contribution() {
    if constexpr(field_has_skip<T, I>()) {
        return 0;
    } else if constexpr(field_has_flatten<T, I>()) {
        using field_t = refl::field_type<T, I>;
        using inner_t = unwrapped_t<field_t>;
        return count_required_fields<inner_t>();
    } else {
        return resolve_field_required<T, I>() ? 1 : 0;
    }
}

template <typename T>
consteval std::size_t count_required_fields() {
    if constexpr(!refl::reflectable_class<std::remove_cvref_t<T>>) {
        return 0;
    } else {
        using U = std::remove_cvref_t<T>;
        constexpr auto N = refl::field_count<U>();
        if constexpr(N == 0) {
            return 0;
        } else {
            return []<std::size_t... Is>(std::index_sequence<Is...>) consteval {
                return (single_required_contribution<U, Is>() + ...);
            }(std::make_index_sequence<N>{});
        }
    }
}

// ─── Fill entries (recursive for flatten, includes aliases) ─────────────────

template <typename T, typename Config, std::size_t N>
consteval void fill_field_entries(std::array<ct_field_entry, N>& entries, std::size_t& pos);

template <typename T, typename Config, std::size_t I, std::size_t N>
consteval void fill_one_field_entry(std::array<ct_field_entry, N>& entries, std::size_t& pos) {
    if constexpr(field_has_skip<T, I>()) {
        // nothing
    } else if constexpr(field_has_flatten<T, I>()) {
        using field_t = refl::field_type<T, I>;
        using inner_t = unwrapped_t<field_t>;
        fill_field_entries<inner_t, Config>(entries, pos);
    } else {
        constexpr auto canonical = serde::schema::canonical_field_name<T, I>();
        constexpr bool has_explicit = field_has_explicit_rename<T, I>();
        constexpr auto kind = resolve_field_kind<T, I>();
        constexpr auto required = resolve_field_required<T, I>();

        name_buf wire;
        if constexpr(has_explicit || !requires { typename Config::field_rename; }) {
            wire = name_buf::from(canonical);
        } else {
            wire = apply_rename_cx<typename Config::field_rename>(canonical);
        }
        entries[pos++] = {wire, kind, required};

        if constexpr(field_has_alias<T, I>()) {
            using field_t = refl::field_type<T, I>;
            using attrs_t = typename field_t::attrs;
            using alias_attr = serde::detail::tuple_find_t<attrs_t, is_alias_attr>;
            for(auto alias_name: alias_attr::names) {
                entries[pos++] = {name_buf::from(alias_name), kind, required};
            }
        }
    }
}

template <typename T, typename Config, std::size_t N>
consteval void fill_field_entries(std::array<ct_field_entry, N>& entries, std::size_t& pos) {
    using U = std::remove_cvref_t<T>;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) consteval {
        (fill_one_field_entry<U, Config, Is>(entries, pos), ...);
    }(std::make_index_sequence<refl::field_count<U>()>{});
}

// ─── Build sorted entry table ───────────────────────────────────────────────

template <typename T, typename Config>
    requires refl::reflectable_class<std::remove_cvref_t<T>>
consteval auto build_field_entries() {
    constexpr std::size_t N = count_match_entries<T>();
    std::array<ct_field_entry, N> entries{};
    std::size_t pos = 0;
    fill_field_entries<T, Config>(entries, pos);

    // Sort by wire name length — groups same-length names together so the
    // fold expression skips non-matching lengths with a single integer compare.
    for(std::size_t i = 1; i < N; i++) {
        for(std::size_t j = i; j > 0 && entries[j].wire_name.len < entries[j - 1].wire_name.len;
            j--) {
            auto tmp = entries[j];
            entries[j] = entries[j - 1];
            entries[j - 1] = tmp;
        }
    }
    return entries;
}

// ─── Fold-based key matcher ─────────────────────────────────────────────────

struct match_result {
    type_kind kind;
    bool required;
};

/// Match a single key against a struct's precomputed wire name table.
/// Uses fold expression with ||, short-circuiting on first match.
/// Length check gates each comparison — compiler optimizes same-length
/// string comparisons into integer/memcmp operations.
template <typename T, typename Config>
    requires refl::reflectable_class<std::remove_cvref_t<T>>
inline auto match_key(std::string_view key) -> std::optional<match_result> {
    static constexpr auto entries = build_field_entries<T, Config>();
    constexpr auto N = entries.size();

    if constexpr(N == 0) {
        return std::nullopt;
    } else {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> std::optional<match_result> {
            std::optional<match_result> r;
            ((key.size() == entries[Is].wire_name.len &&
              key == entries[Is].wire_name.view() &&
              (r.emplace(match_result{entries[Is].kind, entries[Is].required}), true)) ||
             ...);
            return r;
        }(std::make_index_sequence<N>{});
    }
}

}  // namespace detail

// ─── Variant matching ───────────────────────────────────────────────────────

/// Match incoming object keys against variant alternative schemas.
/// Returns the index of the best-matching alternative, or nullopt.
///
/// For each struct alternative, a compile-time entry table is generated (sorted
/// by wire name length). Each incoming key is matched via a fold expression
/// that short-circuits on first hit. Length check provides O(1) filtering;
/// within same-length groups, the compiler optimizes string comparison to
/// integer or memcmp operations.
template <typename Config, typename... Ts>
auto match_variant_by_keys(const std::vector<incoming_field>& incoming)
    -> std::optional<std::size_t> {
    std::optional<std::size_t> best_index;
    std::size_t best_score = 0;

    auto try_one = [&]<std::size_t I>() {
        using Alt = std::variant_alternative_t<I, std::variant<Ts...>>;
        using U = std::remove_cvref_t<Alt>;

        if constexpr(!refl::reflectable_class<U>) {
            return;
        } else {
            static constexpr auto req_count = detail::count_required_fields<U>();

            std::size_t matched = 0;
            std::size_t required_matched = 0;

            for(const auto& f: incoming) {
                auto r = detail::match_key<U, Config>(f.name);
                if(!r) continue;
                if(!detail::kind_compatible(f.kind, r->kind)) {
                    return;
                }
                matched++;
                if(r->required) required_matched++;
            }

            if(required_matched < req_count) return;
            if(!best_index.has_value() || matched > best_score) {
                best_index = I;
                best_score = matched;
            }
        }
    };

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (try_one.template operator()<Is>(), ...);
    }(std::make_index_sequence<sizeof...(Ts)>{});

    return best_index;
}

// ─── Kind-to-index map for primitive dispatch ───────────────────────────────

template <std::size_t N>
struct kind_to_indices_map {
    static constexpr std::size_t max_per_kind = N < 1 ? 1 : N;
    std::array<std::array<std::size_t, max_per_kind>, 8> indices{};
    std::array<std::size_t, 8> count{};
};

template <typename... Ts>
consteval auto build_kind_to_index_map() {
    kind_to_indices_map<sizeof...(Ts)> map{};

    auto try_one = [&]<std::size_t I>() consteval {
        using Alt = std::variant_alternative_t<I, std::variant<Ts...>>;
        auto k = kind_of<Alt>();
        auto ki = static_cast<std::size_t>(k);
        if(ki < 8 && map.count[ki] < sizeof...(Ts)) {
            map.indices[ki][map.count[ki]] = I;
            map.count[ki]++;
        }
    };

    [&]<std::size_t... Is>(std::index_sequence<Is...>) consteval {
        (try_one.template operator()<Is>(), ...);
    }(std::make_index_sequence<sizeof...(Ts)>{});

    return map;
}

}  // namespace eventide::serde::schema
