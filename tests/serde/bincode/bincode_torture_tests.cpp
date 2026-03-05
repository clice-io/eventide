#include <string>

#include "../roundtrip_suite.h"
#include "eventide/zest/zest.h"
#include "eventide/serde/bincode.h"

namespace eventide::serde {

namespace {

using bincode::from_bytes;
using bincode::to_bytes;

auto rt = []<typename T>(const T& input) -> std::expected<T, bincode::error_kind> {
    auto encoded = to_bytes(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(bincode::error_kind::invalid_state);
    }
    return from_bytes<T>(*encoded);
};

TEST_SUITE(serde_bincode_torture) {

TEST_CASE(ultimate_roundtrip){
    SERDE_TEST_ULTIMATE_ROUNDTRIP(rt)} TEST_CASE(variant_and_nullables_roundtrip){
    SERDE_TEST_VARIANT_NULLABLES_ROUNDTRIP(rt)} TEST_CASE(scalars_roundtrip){
    SERDE_TEST_SCALARS_ROUNDTRIP(rt)} TEST_CASE(nested_containers_roundtrip){
    SERDE_TEST_NESTED_CONTAINERS_ROUNDTRIP(rt)} TEST_CASE(empty_containers_roundtrip) {
    SERDE_TEST_EMPTY_CONTAINERS_ROUNDTRIP(rt)
}

};  // TEST_SUITE(serde_bincode_torture)

}  // namespace

}  // namespace eventide::serde
