#pragma once

#include <ranges>
#include <source_location>

#include "cpptrace/cpptrace.hpp"

namespace zest {

inline void print_trace(cpptrace::stacktrace& trace, std::source_location location) {
    auto& frames = trace.frames;
    // auto it = std::ranges::find_if(frames, [&](cpptrace::stacktrace_frame& frame) {
    //     return frame.filename != location.file_name();
    // });
    // frames.erase(it, frames.end());
    trace.print();
}

}  // namespace zest
