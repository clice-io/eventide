#include "kota/zest/assert/trace.h"

#include <algorithm>
#include <format>
#include <print>

#include "kota/support/functional.h"
#include <cpptrace/cpptrace.hpp>

namespace kota::zest {

namespace {

void println_trace(const cpptrace::stacktrace& trace) {
    for(const auto& frame: trace.frames) {
        std::println("{}", frame.to_string());
    }
}

}  // namespace

void print_trace(std::source_location location) {
    auto trace = cpptrace::generate_trace();
    auto& frames = trace.frames;
    if(frames.size() > 1) {
        frames.erase(frames.begin());
    }
    auto it = std::ranges::find_if(frames, [&](const cpptrace::stacktrace_frame& frame) {
        return frame.filename != location.file_name();
    });
    if(it != frames.begin()) {
        frames.erase(it, frames.end());
    }
    println_trace(trace);
}

}  // namespace kota::zest

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace kota::zest {

#ifdef _WIN32

static LONG WINAPI crash_handler(EXCEPTION_POINTERS*) {
    std::println("[  CRASH  ] caught fatal exception, printing stack trace:");
    auto trace = cpptrace::generate_trace();
    for(const auto& frame: trace.frames) {
        std::println("{}", frame.to_string());
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void install_crash_handler() {
    SetUnhandledExceptionFilter(crash_handler);
}

#else

static void crash_handler(int sig) {
    signal(sig, SIG_DFL);
    std::println("[  CRASH  ] caught signal {}, printing stack trace:", sig);
    auto trace = cpptrace::generate_trace();
    for(const auto& frame: trace.frames) {
        std::println("{}", frame.to_string());
    }
    raise(sig);
}

void install_crash_handler() {
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGILL, crash_handler);
#ifdef SIGBUS
    signal(SIGBUS, crash_handler);
#endif
}

#endif

}  // namespace kota::zest

#ifdef __cpp_exceptions
#include <cpptrace/from_current.hpp>

namespace kota::zest {

bool trace_exception(function<void()> cb, bool print) {
    bool ret = false;

    CPPTRACE_TRY {
        CPPTRACE_TRY {
            cb();
        }
        CPPTRACE_CATCH(const std::exception& e) {
            if(print) {
                std::println("[ exception ] {}", e.what());
                println_trace(cpptrace::from_current_exception());
            }
            ret = true;
        }
    }
    CPPTRACE_CATCH(...) {
        if(print) {
            std::println("[ exception ] <non-std exception>");
            println_trace(cpptrace::from_current_exception());
        }
        ret = true;
    }
    return ret;
}

}  // namespace kota::zest
#endif
