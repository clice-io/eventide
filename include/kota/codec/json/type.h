#pragma once

#include "kota/codec/dyn/decode.h"
#include "kota/codec/dyn/document.h"
#include "kota/codec/dyn/encode.h"
#include "kota/codec/visit/context.h"

#if __has_include(<simdjson.h>)
#include "simdjson.h"
#else
#error "simdjson.h is required for the JSON codec backend"
#endif

namespace kota::codec::json {

using ValueKind = dyn::ValueKind;
using Cursor = dyn::Cursor;
using Value = dyn::Value;
using Array = dyn::Array;
using Object = dyn::Object;

using StringBuilder = simdjson::builder::string_builder;

namespace ondemand {

using Parser = simdjson::ondemand::parser;
using Document = simdjson::ondemand::document;
using Value = simdjson::ondemand::value;
using Object = simdjson::ondemand::object;
using Array = simdjson::ondemand::array;
using Type = simdjson::ondemand::json_type;
using NumberType = simdjson::ondemand::number_type;

}  // namespace ondemand

using padded_string = simdjson::padded_string;
using padded_string_view = simdjson::padded_string_view;

constexpr inline auto success = simdjson::SUCCESS;

using error = rich_error;

}  // namespace kota::codec::json
