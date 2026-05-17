#pragma once

#include "kota/codec/bincode/decode.h"
#include "kota/codec/bincode/encode.h"
#include "kota/codec/bincode/type.h"
#include "kota/codec/visit/common.h"

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
struct deserialize_visit<bincode::reader, RawValue, Config> {
    static bool visit(bincode::reader& vis, RawValue& value) {
        std::vector<std::byte> bytes;
        if(!vis.visit_bytes(bytes)) {
            return false;
        }
        value.data.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return true;
    }
};

}  // namespace kota::codec
