#pragma once

#include "kota/codec/bincode/deserializer.h"
#include "kota/codec/bincode/encode.h"
#include "kota/codec/bincode/type.h"
#include "kota/codec/detail/raw_value.h"

namespace kota::codec {

template <typename Config>
struct serialize_visit<bincode::writer, RawValue, Config> {
    static bool visit(bincode::writer& vis, const RawValue& value) {
        auto bytes =
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(value.data.data()),
                                       value.data.size());
        return vis.visit_bytes(bytes);
    }
};

template <typename Config>
struct deserialize_traits<bincode::Deserializer<Config>, RawValue> {
    using error_type = typename bincode::Deserializer<Config>::error_type;

    static auto deserialize(bincode::Deserializer<Config>& deserializer, RawValue& value)
        -> std::expected<void, error_type> {
        std::vector<std::byte> bytes;
        auto status = deserializer.deserialize_bytes(bytes);
        if(!status) {
            return std::unexpected(status.error());
        }
        value.data.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return {};
    }
};

}  // namespace kota::codec
