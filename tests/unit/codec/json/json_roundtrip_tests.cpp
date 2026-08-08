#include "../roundtrip_suite.h"
#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"

namespace kota::codec {

namespace {

using roundtrip::cap;

struct json_adapter {
    constexpr static std::string_view name = "json";
    constexpr static cap caps =
        cap::uint64_full | cap::null_in_seq | cap::variant_plain | cap::attrs | cap::variant_tagged;

    template <typename T>
    static auto run(const T& input) -> std::expected<T, rich_error> {
        auto encoded = json::to_json(input);
        if(!encoded) {
            return std::unexpected(rich_error(encoded.error().to_string()));
        }
        return json::parse<T>(*encoded);
    }
};

TEST_SUITE(serde_json_roundtrip) {

TEST_CASE_GROUP(standard_corpus) {
    roundtrip::register_cases<json_adapter>(add_case);
}

};  // TEST_SUITE(serde_json_roundtrip)

}  // namespace

}  // namespace kota::codec
