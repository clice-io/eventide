#pragma once

#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <stdexcept>

namespace refl::serde {

enum class enum_encoding {
    string,
    integer,
};

template <class E>
struct enum_traits {
    static constexpr bool enabled = false;
};

template <class E>
concept has_enum_traits = enum_traits<E>::enabled;

template <class E>
constexpr std::string_view enum_to_string(E value) {
    static_assert(has_enum_traits<E>);
    if constexpr (enum_traits<E>::encoding == enum_encoding::string) {
        static_assert(requires { enum_traits<E>::mapping; },
                      "enum_traits<E>::mapping is required for string enums.");
        for (const auto& [v, s] : enum_traits<E>::mapping) {
            if (v == value) {
                return s;
            }
        }
        return {};
    } else {
        return {};
    }
}

template <class E>
constexpr bool enum_from_string(std::string_view value, E& out) {
    static_assert(has_enum_traits<E>);
    if constexpr (enum_traits<E>::encoding == enum_encoding::string) {
        static_assert(requires { enum_traits<E>::mapping; },
                      "enum_traits<E>::mapping is required for string enums.");
        for (const auto& [v, s] : enum_traits<E>::mapping) {
            if (s == value) {
                out = v;
                return true;
            }
        }
        return false;
    } else {
        return false;
    }
}

}  // namespace refl::serde

namespace rfl {

template <typename T>
struct Reflector;

template <typename E>
    requires refl::serde::has_enum_traits<E>
struct Reflector<E> {
    using ReflType = std::conditional_t<
        refl::serde::enum_traits<E>::encoding == refl::serde::enum_encoding::integer,
        std::underlying_type_t<E>,
        std::string>;

    static ReflType from(const E& value) {
        if constexpr (refl::serde::enum_traits<E>::encoding ==
                      refl::serde::enum_encoding::integer) {
            return static_cast<ReflType>(value);
        } else {
            return std::string(refl::serde::enum_to_string(value));
        }
    }

    static E to(const ReflType& value) {
        if constexpr (refl::serde::enum_traits<E>::encoding ==
                      refl::serde::enum_encoding::integer) {
            return static_cast<E>(value);
        } else {
            E out{};
            if (refl::serde::enum_from_string(value, out)) {
                return out;
            }
            throw std::runtime_error("Unknown enum value");
        }
    }
};

}  // namespace rfl
