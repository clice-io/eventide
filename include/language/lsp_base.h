#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include "language/json_value.h"

#include <rfl/Flatten.hpp>
#include <rfl/Literal.hpp>
#include <rfl/Rename.hpp>

namespace eventide::language::proto {

using DocumentUri = std::string;
using URI = std::string;

struct no_params {};

using LSPAny = eventide::language::json_value;
using LSPObject = std::map<std::string, LSPAny>;
using LSPArray = std::vector<LSPAny>;

}  // namespace eventide::language::proto
