#include <string>

#include "../roundtrip_suite.h"
#include "eventide/zest/zest.h"
#include "eventide/serde/json/simd_deserializer.h"
#include "eventide/serde/json/simd_serializer.h"

namespace eventide::serde {

namespace {

using json::simd::from_json;
using json::simd::to_json;

auto rt = []<typename T>(const T& input) -> std::expected<T, json::error_kind> {
    auto encoded = to_json(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    return from_json<T>(*encoded);
};

TEST_SUITE(serde_simdjson_torture) {

TEST_CASE(ultimate_roundtrip){
    SERDE_TEST_ULTIMATE_ROUNDTRIP(rt)} TEST_CASE(variant_and_nullables_roundtrip){
    SERDE_TEST_VARIANT_NULLABLES_ROUNDTRIP(rt)} TEST_CASE(scalars_roundtrip){
    SERDE_TEST_SCALARS_ROUNDTRIP(rt)} TEST_CASE(nested_containers_roundtrip){
    SERDE_TEST_NESTED_CONTAINERS_ROUNDTRIP(rt)} TEST_CASE(empty_containers_roundtrip) {
    SERDE_TEST_EMPTY_CONTAINERS_ROUNDTRIP(rt)
}

};  // TEST_SUITE(serde_simdjson_torture)

}  // namespace

}  // namespace eventide::serde
