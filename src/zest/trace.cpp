#include "kota/zest/assert/trace.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <format>
#include <print>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

#ifdef __cpp_exceptions
#include <cpptrace/from_current.hpp>
#endif

#include "kota/support/functional.h"
#include <cpptrace/cpptrace.hpp>

namespace kota::zest {

namespace {

void println_trace(const cpptrace::stacktrace& trace) {
    for(const auto& frame: trace.frames) {
        std::println("{}", frame.to_string());
    }
}

std::size_t format_hex(char* buf, std::size_t buf_size, cpptrace::frame_ptr value) {
    if(buf_size < 3) {
        return 0;
    }
    buf[0] = '0';
    buf[1] = 'x';
    auto [ptr, ec] = std::to_chars(buf + 2, buf + buf_size, value, 16);
    if(ec != std::errc{}) {
        return 0;
    }
    return static_cast<std::size_t>(ptr - buf);
}

std::size_t format_frame(char* buf, std::size_t buf_size, const cpptrace::object_frame& frame) {
    std::size_t pos = 0;

    const char prefix[] = "__ZEST_FRAME__:";
    std::memcpy(buf + pos, prefix, sizeof(prefix) - 1);
    pos += sizeof(prefix) - 1;

    pos += format_hex(buf + pos, buf_size - pos, frame.raw_address);
    buf[pos++] = ':';

    pos += format_hex(buf + pos, buf_size - pos, frame.object_address);
    buf[pos++] = ':';

    auto path_len = frame.object_path.size();
    if(pos + path_len + 1 < buf_size) {
        std::memcpy(buf + pos, frame.object_path.data(), path_len);
        pos += path_len;
    }
    buf[pos++] = '\n';

    return pos;
}

#ifndef _WIN32
void safe_write_all(int fd, const char* data, std::size_t len) {
    while(len > 0) {
        auto written = write(fd, data, len);
        if(written <= 0) {
            break;
        }
        data += written;
        len -= static_cast<std::size_t>(written);
    }
}
#endif

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

#ifdef _WIN32

static void write_object_trace(HANDLE out, const cpptrace::object_trace& trace) {
    DWORD written;
    const char begin_marker[] = "__ZEST_TRACE_BEGIN__\n";
    const char end_marker[] = "__ZEST_TRACE_END__\n";
    WriteFile(out, begin_marker, sizeof(begin_marker) - 1, &written, nullptr);

    for(const auto& frame: trace.frames) {
        char buf[CPPTRACE_PATH_MAX + 256];
        auto pos = format_frame(buf, sizeof(buf), frame);
        WriteFile(out, buf, static_cast<DWORD>(pos), &written, nullptr);
    }

    WriteFile(out, end_marker, sizeof(end_marker) - 1, &written, nullptr);
}

static LONG WINAPI crash_handler(EXCEPTION_POINTERS*) {
    // Object trace only (no DWARF) to avoid TSAN-spinning during resolution.
    auto trace = cpptrace::generate_object_trace();

    const char header[] = "[  CRASH  ] caught fatal exception, printing stack trace:\n";
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(out, header, sizeof(header) - 1, &written, nullptr);
    write_object_trace(out, trace);
    return EXCEPTION_CONTINUE_SEARCH;
}

void install_crash_handler() {
    SetUnhandledExceptionFilter(crash_handler);
}

#else

static void write_object_trace(int fd, const cpptrace::object_trace& trace) {
    const char begin_marker[] = "__ZEST_TRACE_BEGIN__\n";
    const char end_marker[] = "__ZEST_TRACE_END__\n";
    safe_write_all(fd, begin_marker, sizeof(begin_marker) - 1);

    for(const auto& frame: trace.frames) {
        char buf[CPPTRACE_PATH_MAX + 256];
        auto pos = format_frame(buf, sizeof(buf), frame);
        safe_write_all(fd, buf, pos);
    }

    safe_write_all(fd, end_marker, sizeof(end_marker) - 1);
}

static void crash_handler(int sig) {
    signal(sig, SIG_DFL);

    // Object trace only (no DWARF) to avoid TSAN-spinning during resolution.
    auto trace = cpptrace::generate_object_trace();

    const char msg1[] = "[  CRASH  ] caught signal ";
    safe_write_all(STDOUT_FILENO, msg1, sizeof(msg1) - 1);
    char sigbuf[16];
    if(auto [ptr, ec] = std::to_chars(sigbuf, sigbuf + sizeof(sigbuf), sig); ec == std::errc{}) {
        safe_write_all(STDOUT_FILENO, sigbuf, static_cast<std::size_t>(ptr - sigbuf));
    }
    const char msg2[] = ", printing stack trace:\n";
    safe_write_all(STDOUT_FILENO, msg2, sizeof(msg2) - 1);

    write_object_trace(STDOUT_FILENO, trace);
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

#ifdef __cpp_exceptions

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

#endif

}  // namespace kota::zest
