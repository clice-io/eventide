#pragma once

#include "kota/codec/visit/context.h"

#if __has_include(<simdjson.h>)
#include "simdjson.h"
#else
#error "simdjson.h is required for the JSON codec backend"
#endif

namespace kota::codec::json {

/// Format tag: scopes a meta::repr specialization to the JSON backend
/// (meta::repr<T, codec::json::format>).
struct format {};

using StringBuilder = simdjson::builder::string_builder;

namespace ondemand {

using Parser = simdjson::ondemand::parser;
using Document = simdjson::ondemand::document;
using Value = simdjson::ondemand::value;
using Type = simdjson::ondemand::json_type;
using NumberType = simdjson::ondemand::number_type;

}  // namespace ondemand

using padded_string = simdjson::padded_string;

constexpr inline auto success = simdjson::SUCCESS;

using error = rich_error;

}  // namespace kota::codec::json
