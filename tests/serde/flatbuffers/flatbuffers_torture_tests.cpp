#if __has_include(<flatbuffers/flexbuffers.h>) && __has_include(<flatbuffers/flatbuffers.h>)

#include <string>

#include "../roundtrip_suite.h"
#include "eventide/zest/zest.h"
#include "eventide/serde/flatbuffers/binary.h"
#include "eventide/serde/flatbuffers/flex_deserializer.h"
#include "eventide/serde/flatbuffers/flex_serializer.h"

namespace eventide::serde {

namespace {

auto rt_flex = []<typename T>(const T& input) -> std::expected<T, flex::error_code> {
    auto encoded = flex::to_flatbuffer(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(flex::error_code::invalid_state);
    }
    return flex::from_flatbuffer<T>(*encoded);
};

auto rt_binary =
    []<typename T>(const T& input) -> std::expected<T, flatbuffers::binary::object_error_code> {
    auto encoded = flatbuffers::binary::to_flatbuffer(input);
    if(!encoded) {
        return std::unexpected(encoded.error());
    }
    if(encoded->empty()) {
        return std::unexpected(flatbuffers::binary::object_error_code::invalid_state);
    }
    return flatbuffers::binary::from_flatbuffer<T>(*encoded);
};

TEST_SUITE(serde_flatbuffers_torture) {

TEST_CASE(ultimate_roundtrip_flex){
    SERDE_TEST_ULTIMATE_ROUNDTRIP(rt_flex)} TEST_CASE(variant_and_nullables_roundtrip_flex){
    SERDE_TEST_VARIANT_NULLABLES_ROUNDTRIP(rt_flex)} TEST_CASE(scalars_roundtrip_flex){
    SERDE_TEST_SCALARS_ROUNDTRIP(rt_flex)} TEST_CASE(nested_containers_roundtrip_flex){
    SERDE_TEST_NESTED_CONTAINERS_ROUNDTRIP(rt_flex)} TEST_CASE(empty_containers_roundtrip_flex){
    SERDE_TEST_EMPTY_CONTAINERS_ROUNDTRIP(rt_flex)}

TEST_CASE(ultimate_roundtrip_binary){
    SERDE_TEST_ULTIMATE_ROUNDTRIP(rt_binary)} TEST_CASE(variant_and_nullables_roundtrip_binary){
    SERDE_TEST_VARIANT_NULLABLES_ROUNDTRIP(rt_binary)} TEST_CASE(scalars_roundtrip_binary){
    SERDE_TEST_SCALARS_ROUNDTRIP(rt_binary)} TEST_CASE(nested_containers_roundtrip_binary){
    SERDE_TEST_NESTED_CONTAINERS_ROUNDTRIP(
        rt_binary)} TEST_CASE(empty_containers_roundtrip_binary) {
    SERDE_TEST_EMPTY_CONTAINERS_ROUNDTRIP(rt_binary)
}

};  // TEST_SUITE(serde_flatbuffers_torture)

}  // namespace

}  // namespace eventide::serde

#endif
