#pragma once

#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "spelling.h"

namespace serde {

// Wrap or inherit depending on whether T is an aggregate class.
template <typename T>
concept wrap_type = !std::is_class_v<T> || std::is_final_v<T>;

// Aggregate class that can be inherited without losing aggregate-ness.
template <typename T>
concept inherit_type = std::is_aggregate_v<T> && !wrap_type<T>;

// Non-aggregate class where inheriting and reusing constructors is desired.
template <typename T>
concept inherit_use_type = !std::is_aggregate_v<T> && !wrap_type<T>;

template <typename T, typename... Attrs>
struct annotate;

template <wrap_type T, typename... Attrs>
struct annotate<T, Attrs...> {
    T value;

    constexpr annotate() = default;

    template <typename U>
        requires (!std::same_as<std::remove_cvref_t<U>, annotate> && std::constructible_from<T, U>)
    constexpr annotate(U&& raw) : value(std::forward<U>(raw)) {}

    operator T&() {
        return value;
    }

    operator const T&() const {
        return value;
    }

    template <typename U>
        requires (!std::same_as<std::remove_cvref_t<U>, annotate> && std::assignable_from<T&, U>)
    constexpr annotate& operator=(U&& raw) {
        value = std::forward<U>(raw);
        return *this;
    }

    using annotated_type = T;
    using attrs = std::tuple<Attrs...>;
};

template <inherit_type T, typename... Attrs>
struct annotate<T, Attrs...> : T {
    using annotated_type = T;
    using attrs = std::tuple<Attrs...>;
};

template <inherit_use_type T, typename... Attrs>
struct annotate<T, Attrs...> : T {
    using T::T;
    using annotated_type = T;
    using attrs = std::tuple<Attrs...>;
};

template <std::size_t N>
struct fixed_string : std::array<char, N + 1> {
    constexpr fixed_string(const char* str) {
        for(std::size_t i = 0; i < N; ++i) {
            this->data()[i] = str[i];
        }
        this->data()[N] = '\0';
    }

    template <std::size_t M>
    constexpr fixed_string(const char (&str)[M]) {
        for(std::size_t i = 0; i < N; ++i) {
            this->data()[i] = str[i];
        }
        this->data()[N] = '\0';
    }

    constexpr static auto size() {
        return N;
    }

    constexpr operator std::string_view() const {
        return std::string_view(this->data(), N);
    }
};

template <std::size_t M>
fixed_string(const char (&)[M]) -> fixed_string<M - 1>;

namespace attr {

template <fixed_string... Names>
struct alias {
    constexpr inline static std::array names = {std::string_view(Names)...};
};

struct flatten {};

template <fixed_string Name>
struct literal {
    constexpr inline static std::string_view name = Name;
};

template <fixed_string Name>
struct rename {
    constexpr inline static std::string_view name = Name;
};

template <typename Policy = rename_policy::lower_camel>
struct enum_string {};

struct skip {};

template <typename Pred>
struct skip_if {};

}  // namespace attr

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

namespace detail {

template <typename E, typename Policy = rename_policy::lower_camel>
std::string map_enum_to_string(E value) {
    return spelling::map_enum_to_string<E, Policy>(value);
}

template <typename E, typename Policy = rename_policy::lower_camel>
constexpr std::optional<E> map_string_to_enum(std::string_view value) {
    return spelling::map_string_to_enum<E, Policy>(value);
}

}  // namespace detail

template <typename T>
using skip = annotate<T, attr::skip>;

template <typename T>
using flatten = annotate<T, attr::flatten>;

template <typename T, fixed_string Name>
using literal = annotate<T, attr::literal<Name>>;

template <typename T, fixed_string Name>
using rename = annotate<T, attr::rename<Name>>;

template <typename T, fixed_string Name, fixed_string... AliasNames>
using rename_alias = annotate<T, attr::rename<Name>, attr::alias<AliasNames...>>;

template <typename T, fixed_string... Names>
using alias = annotate<T, attr::alias<Names...>>;

template <typename T, typename Pred>
using skip_if = annotate<T, attr::skip_if<Pred>>;

template <typename E, typename Policy = rename_policy::lower_camel>
using enum_string = annotate<E, attr::enum_string<Policy>>;

template <typename T>
using skip_if_none = annotate<std::optional<T>, attr::skip_if<pred::optional_none>>;

template <typename T>
using skip_if_empty = annotate<T, attr::skip_if<pred::empty>>;

template <typename T>
using skip_if_default = annotate<T, attr::skip_if<pred::default_value>>;

}  // namespace serde
