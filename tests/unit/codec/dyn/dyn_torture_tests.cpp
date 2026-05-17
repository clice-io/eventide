#include "../standard_case_suite.h"
#include "kota/zest/zest.h"
#include "kota/codec/dyn/dyn.h"

namespace kota::codec {

namespace {

using dyn::from_content;
using dyn::to_content;

auto rt = []<typename T>(const T& input) -> std::expected<T, rich_error> {
    auto encoded = to_content(input);
    if(!encoded) {
        return std::unexpected(rich_error(encoded.error().to_string()));
    }
    return from_content<T>(*encoded);
};

TEST_SUITE(serde_dyn_standard) {

SERDE_STANDARD_TEST_CASES_PRIMITIVES(rt)
SERDE_STANDARD_TEST_CASES_NUMERIC_BOUNDARIES(rt)
SERDE_STANDARD_TEST_CASES_TUPLE_LIKE(rt)
SERDE_STANDARD_TEST_CASES_SEQUENCE_SET(rt)
SERDE_STANDARD_TEST_CASES_MAPS(rt)
SERDE_STANDARD_TEST_CASES_OPTIONAL(rt)
SERDE_STANDARD_TEST_CASES_POINTERS_WIRE_SAFE(rt)
SERDE_STANDARD_TEST_CASES_VARIANT_WIRE_SAFE(rt)
SERDE_STANDARD_TEST_CASES_COMPLEX(rt)

};  // TEST_SUITE(serde_dyn_standard)

}  // namespace

}  // namespace kota::codec
