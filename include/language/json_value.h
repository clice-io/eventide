#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <rfl/Generic.hpp>
#include <rfl/json/Reader.hpp>

#if __has_include(<yyjson.h>)
#include <yyjson.h>
#else
#include <rfl/thirdparty/yyjson.h>
#endif

namespace eventide::language {

struct json_value {
    json_value() = default;

    static json_value from_json_obj(rfl::json::Reader::InputVarType value);

    rfl::Generic to_generic() const;

    std::string to_string() const;

    yyjson_mut_val* value() const noexcept {
        return value_;
    }

private:
    friend struct json_null;

    json_value(std::shared_ptr<yyjson_mut_doc> doc, yyjson_mut_val* value) :
        doc_(std::move(doc)), value_(value) {}

    std::shared_ptr<yyjson_mut_doc> doc_{};
    yyjson_mut_val* value_ = nullptr;
};

struct json_null {
    static json_null from_json_obj(rfl::json::Reader::InputVarType value);
};

}  // namespace eventide::language

namespace rfl {

template <typename T>
struct Reflector;

template <>
struct Reflector<eventide::language::json_value> {
    using ReflType = rfl::Generic;

    static ReflType from(const eventide::language::json_value& value) {
        return value.to_generic();
    }
};

template <>
struct Reflector<eventide::language::json_null> {
    using ReflType = rfl::Generic;

    static ReflType from(const eventide::language::json_null&) {
        return rfl::Generic(rfl::Generic::Null);
    }
};

}  // namespace rfl
