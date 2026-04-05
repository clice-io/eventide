#if __has_include(<toml++/toml.hpp>)

#include <string>

#include "../standard_case_suite.h"
#include "eventide/zest/zest.h"
#include "eventide/serde/toml.h"

namespace eventide::serde {

namespace {

using toml::parse;
using toml::to_string;

auto rt = []<typename T>(const T& input) -> std::expected<T, toml::error> {
    auto encoded = to_string(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    return parse<T>(*encoded);
};

TEST_SUITE(serde_toml_standard_attrs) {

SERDE_STANDARD_TEST_CASES_VARIANT_TOML_SAFE(rt)
SERDE_STANDARD_TEST_CASES_ATTRS(rt)
SERDE_STANDARD_TEST_CASES_BEHAVIOR_ATTRS(rt)
SERDE_STANDARD_TEST_CASES_TAGGED_VARIANTS_NO_INTERNAL(rt)
// Internally tagged variants require content::Deserializer for buffered
// field replay, which the TOML backend does not implement.
// RawValue is not supported: TOML has no raw-passthrough serialization API.

};  // TEST_SUITE(serde_toml_standard_attrs)

}  // namespace

}  // namespace eventide::serde

#endif
