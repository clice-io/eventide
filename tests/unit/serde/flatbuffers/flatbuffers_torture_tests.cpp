#if __has_include(<flatbuffers/flatbuffers.h>)

#include <string>

#include "../standard_case_suite.h"
#include "eventide/zest/zest.h"
#include "eventide/serde/flatbuffers/flatbuffers.h"

namespace eventide::serde {

namespace {

auto rt = []<typename T>(const T& input) -> std::expected<T, flatbuffers::object_error_code> {
    auto encoded = flatbuffers::to_flatbuffer(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(flatbuffers::object_error_code::invalid_state);
    }
    return flatbuffers::from_flatbuffer<T>(*encoded);
};

TEST_SUITE(serde_flatbuffers_standard) {

SERDE_STANDARD_TEST_CASES_PRIMITIVES(rt)
SERDE_STANDARD_TEST_CASES_NUMERIC_BOUNDARIES(rt)
SERDE_STANDARD_TEST_CASES_TUPLE_LIKE(rt)
SERDE_STANDARD_TEST_CASES_SEQUENCE_SET(rt)
SERDE_STANDARD_TEST_CASES_MAPS(rt)
SERDE_STANDARD_TEST_CASES_OPTIONAL(rt)
SERDE_STANDARD_TEST_CASES_POINTERS_WIRE_SAFE(rt)
SERDE_STANDARD_TEST_CASES_VARIANT_WIRE_SAFE(rt)
// BEHAVIOR_ATTRS: flatbuffers strips behavior annotations (with<>/as<>) in
// its own decode path, so adapter transformations are not applied on deser.
// TAGGED_VARIANTS: flatbuffers uses schema-based field layout, not runtime
// string-keyed tags; external/adjacent/internal tagging is not applicable.
// ATTRS: annotation<T, flatten/rename_all/deny_unknown_fields> triggers
// structured-binding decomposition errors in the reflection system.
// RawValue is not supported: no serialize/deserialize traits for flatbuffers.
SERDE_STANDARD_TEST_CASES_COMPLEX(rt)

};  // TEST_SUITE(serde_flatbuffers_standard)

}  // namespace

}  // namespace eventide::serde

#endif
