#include "../standard_case_suite.h"
#include "kota/zest/zest.h"
#include "kota/codec/capnp/capnp.h"

namespace kota::codec {

namespace {

auto rt = []<typename T>(const T& input) -> std::expected<T, capnp::object_error_code> {
    auto encoded = capnp::to_capnp(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(capnp::object_error_code::invalid_state);
    }
    return capnp::from_capnp<T>(*encoded);
};

TEST_SUITE(serde_capnp_standard) {

SERDE_STANDARD_TEST_CASES_PRIMITIVES(rt)
SERDE_STANDARD_TEST_CASES_NUMERIC_BOUNDARIES(rt)
SERDE_STANDARD_TEST_CASES_TUPLE_LIKE(rt)
SERDE_STANDARD_TEST_CASES_SEQUENCE_SET(rt)
SERDE_STANDARD_TEST_CASES_MAPS(rt)
SERDE_STANDARD_TEST_CASES_OPTIONAL(rt)
SERDE_STANDARD_TEST_CASES_POINTERS_WIRE_SAFE(rt)
SERDE_STANDARD_TEST_CASES_VARIANT_WIRE_SAFE(rt)
SERDE_STANDARD_TEST_CASES_TAGGED_VARIANTS(rt)
SERDE_STANDARD_TEST_CASES_COMPLEX(rt)

};  // TEST_SUITE(serde_capnp_standard)

}  // namespace

}  // namespace kota::codec
