#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace kota::ipc {

enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
};

using LogCallback = std::function<void(LogLevel, std::string)>;

}  // namespace kota::ipc
