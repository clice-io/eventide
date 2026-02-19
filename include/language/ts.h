#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "serde/serde.h"

namespace language::protocol {

/// For `undefined | bool` .
using optional_bool = serde::skip_if_default<bool>;

/// For `undefined | T` .
template <typename T>
using optional = serde::skip_if_none<T>;

/// For `a: T | null`
template <typename T>
using nullable = std::optional<T>;

/// For `A | B | ...`
using std::variant;

/// For `undefined | (A | B | ...)`
template <typename... Ts>
using optional_variant = optional<variant<Ts...>>;

/// For multiple inherit.
using serde::flatten;

/// For closed string enum.
using serde::enum_string;

/// For empty object literal.
struct LspEmptyObject {};

/// The LSP any type.
/// Please note that strictly speaking a property with the value `undefined`
/// can't be converted into JSON preserving the property name. However for
/// convenience it is allowed and assumed that all these properties are
/// optional as well.
/// @since 3.17.0
struct LSPAny;

/// LSP arrays.
/// @since 3.17.0
using LSPArray = std::vector<LSPAny>;

/// LSP object definition.
/// @since 3.17.0
using LSPObject = std::unordered_map<std::string, LSPAny>;

using LSPVariant = std::variant<LSPObject,
                                LSPArray,
                                std::string,
                                std::int64_t,
                                std::uint32_t,
                                double,
                                bool,
                                std::nullptr_t>;

struct LSPAny : LSPVariant {
    using LSPVariant::LSPVariant;
    using LSPVariant::operator=;
};

struct LSPEmpty {};

// LSP request/notification traits (Params -> method/result).
template <typename Params>
struct RequestTraits;

template <typename Params>
struct NotificationTraits;

using boolean = bool;
using integer = std::int32_t;
using uinteger = std::uint32_t;
using decimal = double;
using string = std::string;
using null = std::nullptr_t;
using URI = string;
using DocumentUri = string;

using RequestID = variant<integer, string>;

struct IncomingMessage {
    optional<string> method;
    optional<RequestID> id;
    optional<string> params_json;
};

struct ResponseError {
    integer code = 0;
    string message;
    optional<LSPAny> data = {};
};

}  // namespace language::protocol

namespace serde {

template <serializer_like S>
struct serialize_traits<S, language::protocol::LSPAny> {
    using value_type = typename S::value_type;
    using error_type = typename S::error_type;

    static auto serialize(S& serializer, const language::protocol::LSPAny& value)
        -> std::expected<value_type, error_type> {
        const auto& variant = static_cast<const language::protocol::LSPVariant&>(value);
        return std::visit([&](const auto& item) { return serde::serialize(serializer, item); },
                          variant);
    }
};

template <deserializer_like D>
struct deserialize_traits<D, language::protocol::LSPAny> {
    using error_type = typename D::error_type;

    static auto deserialize(D& deserializer, language::protocol::LSPAny& value)
        -> std::expected<void, error_type> {
        language::protocol::LSPVariant variant{};
        auto status = serde::deserialize(deserializer, variant);
        if(!status) {
            return std::unexpected(status.error());
        }
        std::visit([&](auto&& item) { value = std::forward<decltype(item)>(item); },
                   std::move(variant));
        return {};
    }
};

}  // namespace serde
