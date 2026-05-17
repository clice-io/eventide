#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "kota/support/naming.h"
#include "kota/support/type_traits.h"
#include "kota/meta/enum.h"

namespace kota::codec {

namespace spelling {

namespace detail {

template <typename Mapped>
std::string to_string_storage(Mapped&& mapped) {
    using mapped_t = std::remove_cvref_t<Mapped>;
    if constexpr(std::same_as<mapped_t, std::string>) {
        return std::forward<Mapped>(mapped);
    } else if constexpr(std::convertible_to<Mapped, std::string_view>) {
        return std::string(static_cast<std::string_view>(mapped));
    } else {
        static_assert(dependent_false<mapped_t>,
                      "rename policy must return std::string or string-like value");
        return {};
    }
}

}  // namespace detail

namespace rename_policy {

using identity = naming::rename_policy::identity;
using lower_snake = naming::rename_policy::lower_snake;
using lower_camel = naming::rename_policy::lower_camel;
using upper_camel = naming::rename_policy::upper_camel;
using upper_snake = naming::rename_policy::upper_snake;
using upper_case = naming::rename_policy::upper_case;

}  // namespace rename_policy

template <typename Policy>
std::string apply_rename_policy(bool is_serialize, std::string_view value) {
    if constexpr(requires(Policy policy) { policy(is_serialize, value); }) {
        return detail::to_string_storage(Policy{}(is_serialize, value));
    } else {
        static_assert(dependent_false<Policy>,
                      "rename policy must support operator()(bool, std::string_view)");
    }
}

template <typename E, typename Policy = rename_policy::lower_camel>
std::string map_enum_to_string(E value) {
    static_assert(std::is_enum_v<E>, "map_enum_to_string requires an enum type");
    return apply_rename_policy<Policy>(true, meta::enum_name(value));
}

template <typename E, typename Policy = rename_policy::lower_camel>
auto enum_strings() -> const std::vector<std::string>& {
    static_assert(std::is_enum_v<E>, "enum_strings requires an enum type");
    const static auto names = [] {
        std::vector<std::string> values;
        values.reserve(meta::reflection<E>::member_values.size());
        for(const auto value: meta::reflection<E>::member_values) {
            values.push_back(map_enum_to_string<E, Policy>(value));
        }
        return values;
    }();
    return names;
}

template <typename E, typename Policy = rename_policy::lower_camel>
constexpr std::optional<E> map_string_to_enum(std::string_view value) {
    static_assert(std::is_enum_v<E>, "map_string_to_enum requires an enum type");
    auto mapped = apply_rename_policy<Policy>(false, value);
    auto try_parse = [](std::string_view candidate) -> std::optional<E> {
        if(auto parsed = meta::enum_value<E>(candidate)) {
            return parsed;
        }

        // Keyword-safe fallback for generated enum members like `Delete_`/`Import_`.
        auto keyword_suffixed = std::string(candidate);
        keyword_suffixed.push_back('_');
        if(auto parsed = meta::enum_value<E>(keyword_suffixed)) {
            return parsed;
        }

        if(!candidate.empty() && naming::is_digit(candidate.front())) {
            auto underscored = std::string("_") + std::string(candidate);
            if(auto parsed = meta::enum_value<E>(underscored)) {
                return parsed;
            }

            auto value_prefixed = std::string("V") + std::string(candidate);
            if(auto parsed = meta::enum_value<E>(value_prefixed)) {
                return parsed;
            }
        }

        return std::nullopt;
    };

    if(auto parsed = try_parse(mapped)) {
        return parsed;
    }

    auto lower_camel = naming::snake_to_camel(mapped, false);
    if(auto parsed = try_parse(lower_camel)) {
        return parsed;
    }

    auto upper_camel = naming::snake_to_camel(mapped, true);
    if(auto parsed = try_parse(upper_camel)) {
        return parsed;
    }

    return std::nullopt;
}

}  // namespace spelling

namespace rename_policy {

using identity = spelling::rename_policy::identity;
using lower_snake = spelling::rename_policy::lower_snake;
using lower_camel = spelling::rename_policy::lower_camel;
using upper_camel = spelling::rename_policy::upper_camel;
using upper_snake = spelling::rename_policy::upper_snake;
using upper_case = spelling::rename_policy::upper_case;

}  // namespace rename_policy

}  // namespace kota::codec
