#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <rfl/Generic.hpp>
#include <rfl/Literal.hpp>

namespace eventide::language::proto {

using DocumentUri = std::string;
using URI = std::string;

struct no_params {};

using LSPAny = rfl::Generic;
using LSPObject = std::map<std::string, LSPAny>;
using LSPArray = std::vector<LSPAny>;

}  // namespace eventide::language::proto
