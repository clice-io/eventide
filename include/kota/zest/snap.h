#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>

#include "kota/zest/detail/check.h"
#include "kota/zest/detail/registry.h"
#include "kota/zest/detail/trace.h"

namespace kota::codec::content {
class Value;
}

namespace kota::zest::snap {

enum class Result {
    matched,
    created,
    updated,
    mismatch,
};

void reset_counter();
std::uint32_t next_counter();

struct GlobContext {
    std::string stem;
};

std::optional<GlobContext>& current_glob_context();

Result check(std::string_view content,
             std::string_view expr,
             std::source_location loc);

bool check_inline(std::string_view actual,
                  std::string_view expected,
                  std::string_view expr,
                  std::source_location loc);

std::string prettify_json(std::string_view compact);

std::string value_to_yaml(const codec::content::Value& value);

void glob(std::string_view pattern,
          std::string_view source_file,
          std::function<void(const std::filesystem::path&)> callback);

std::filesystem::path snapshot_path(std::string_view source_file,
                                    std::string_view suite,
                                    std::string_view test,
                                    std::uint32_t counter,
                                    const std::optional<GlobContext>& glob_ctx);

}  // namespace kota::zest::snap

// clang-format off

#define ZEST_SNAPSHOT_CHECK(return_action, ...)                                                     \
    do {                                                                                           \
        auto _zest_snap_str_ = ::kota::zest::pretty_dump(__VA_ARGS__);                             \
        auto _zest_snap_result_ = ::kota::zest::snap::check(                                       \
            _zest_snap_str_, #__VA_ARGS__, std::source_location::current());                        \
        if(_zest_snap_result_ == ::kota::zest::snap::Result::mismatch) {                           \
            ::kota::zest::print_trace(std::source_location::current());                            \
            ::kota::zest::failure();                                                               \
            return_action;                                                                         \
        }                                                                                          \
    } while(0)

#define EXPECT_SNAPSHOT(...) ZEST_SNAPSHOT_CHECK((void)0, __VA_ARGS__)
#define ASSERT_SNAPSHOT(...) ZEST_SNAPSHOT_CHECK(return, __VA_ARGS__)

#define ZEST_INLINE_SNAPSHOT_CHECK(return_action, value, expected)                                  \
    do {                                                                                           \
        auto _zest_snap_str_ = ::kota::zest::pretty_dump(value);                                   \
        if(!::kota::zest::snap::check_inline(                                                      \
            _zest_snap_str_, (expected), #value, std::source_location::current())) {                \
            ::kota::zest::print_trace(std::source_location::current());                            \
            ::kota::zest::failure();                                                               \
            return_action;                                                                         \
        }                                                                                          \
    } while(0)

#define EXPECT_INLINE_SNAPSHOT(value, expected) ZEST_INLINE_SNAPSHOT_CHECK((void)0, value, expected)
#define ASSERT_INLINE_SNAPSHOT(value, expected) ZEST_INLINE_SNAPSHOT_CHECK(return, value, expected)

#define ZEST_JSON_SNAPSHOT_CHECK(return_action, ...)                                                \
    do {                                                                                           \
        auto _zest_json_result_ = ::kota::codec::json::to_json(__VA_ARGS__);                       \
        auto _zest_snap_str_ = _zest_json_result_                                                  \
            ? ::kota::zest::snap::prettify_json(*_zest_json_result_)                               \
            : std::string("<json serialization failed>");                                          \
        auto _zest_snap_result_ = ::kota::zest::snap::check(                                       \
            _zest_snap_str_, #__VA_ARGS__, std::source_location::current());                        \
        if(_zest_snap_result_ == ::kota::zest::snap::Result::mismatch) {                           \
            ::kota::zest::print_trace(std::source_location::current());                            \
            ::kota::zest::failure();                                                               \
            return_action;                                                                         \
        }                                                                                          \
    } while(0)

#define EXPECT_JSON_SNAPSHOT(...) ZEST_JSON_SNAPSHOT_CHECK((void)0, __VA_ARGS__)
#define ASSERT_JSON_SNAPSHOT(...) ZEST_JSON_SNAPSHOT_CHECK(return, __VA_ARGS__)

#define ZEST_YAML_SNAPSHOT_CHECK(return_action, ...)                                                \
    do {                                                                                           \
        ::kota::codec::content::Serializer<> _zest_yaml_ser_;                                      \
        auto _zest_yaml_val_ = ::kota::codec::serialize(_zest_yaml_ser_, (__VA_ARGS__));            \
        auto _zest_snap_str_ = _zest_yaml_val_                                                     \
            ? ::kota::zest::snap::value_to_yaml(*_zest_yaml_val_)                                  \
            : std::string("<yaml serialization failed>");                                          \
        auto _zest_snap_result_ = ::kota::zest::snap::check(                                       \
            _zest_snap_str_, #__VA_ARGS__, std::source_location::current());                        \
        if(_zest_snap_result_ == ::kota::zest::snap::Result::mismatch) {                           \
            ::kota::zest::print_trace(std::source_location::current());                            \
            ::kota::zest::failure();                                                               \
            return_action;                                                                         \
        }                                                                                          \
    } while(0)

#define EXPECT_YAML_SNAPSHOT(...) ZEST_YAML_SNAPSHOT_CHECK((void)0, __VA_ARGS__)
#define ASSERT_YAML_SNAPSHOT(...) ZEST_YAML_SNAPSHOT_CHECK(return, __VA_ARGS__)

#define SNAPSHOT_GLOB(pattern, ...)                                                                 \
    ::kota::zest::snap::glob(pattern,                                                              \
                             std::source_location::current().file_name(),                           \
                             __VA_ARGS__)

// clang-format on
