#pragma once

#include <optional>

#include "spelling.h"
#include "eventide/common/fixed_string.h"
#include "eventide/serde/annotation.h"
#include "eventide/serde/attrs/alias.h"
#include "eventide/serde/attrs/context.h"
#include "eventide/serde/attrs/enum_string.h"
#include "eventide/serde/attrs/flatten.h"
#include "eventide/serde/attrs/literal.h"
#include "eventide/serde/attrs/rename.h"
#include "eventide/serde/attrs/skip.h"
#include "eventide/serde/attrs/skip_if.h"

namespace eventide::serde {

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

template <typename T>
using skip = annotation<T, attr::skip>;

template <typename T>
using flatten = annotation<T, attr::flatten>;

template <typename T, fixed_string Name>
using literal = annotation<T, attr::literal<Name>>;

template <typename T, fixed_string Name>
using rename = annotation<T, attr::rename<Name>>;

template <typename T, fixed_string Name, fixed_string... AliasNames>
using rename_alias = annotation<T, attr::rename<Name>, attr::alias<AliasNames...>>;

template <typename T, fixed_string... Names>
using alias = annotation<T, attr::alias<Names...>>;

template <typename T, typename Pred>
using skip_if = annotation<T, attr::skip_if<Pred>>;

template <typename E, typename Policy = rename_policy::lower_camel>
using enum_string = annotation<E, attr::enum_string<Policy>>;

template <typename T>
using skip_if_none = annotation<std::optional<T>, attr::skip_if<pred::optional_none>>;

template <typename T>
using skip_if_empty = annotation<T, attr::skip_if<pred::empty>>;

template <typename T>
using skip_if_default = annotation<T, attr::skip_if<pred::default_value>>;

}  // namespace eventide::serde
