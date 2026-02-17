#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
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

using boolean = bool;
using integer = std::int64_t;
using uinteger = std::uint32_t;
using decimal = double;
using string = std::string;
using null = std::nullptr_t;
using URI = string;
using DocumentUri = string;

}  // namespace language::protocol
