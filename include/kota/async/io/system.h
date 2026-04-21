#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "kota/async/vocab/error.h"

namespace kota::sys {

/// System memory information (all values in bytes).
struct memory_info {
    /// Total physical memory installed on the system.
    std::uint64_t total = 0;

    /// Physical memory not currently in use.
    std::uint64_t free = 0;

    /// Memory available to the process, accounting for cgroup/container
    /// limits.  Falls back to free memory when no limit is set.
    std::uint64_t available = 0;

    /// Memory limit imposed by the OS (cgroups on Linux, Job Objects on
    /// Windows).  Zero when no constraint is active.
    std::uint64_t constrained = 0;
};

/// Per-CPU core timing snapshot (all values in milliseconds).
struct cpu_times {
    /// Time spent running user-space processes.
    std::uint64_t user = 0;

    /// Time spent running niced user-space processes.
    std::uint64_t nice = 0;

    /// Time spent running kernel-space code.
    std::uint64_t sys = 0;

    /// Time spent idle.
    std::uint64_t idle = 0;

    /// Time spent servicing hardware interrupts.
    std::uint64_t irq = 0;
};

/// Information about a single logical CPU core.
struct cpu_info {
    /// CPU model name (e.g. "Intel(R) Core(TM) i7-10700K").
    std::string model;

    /// Clock speed in MHz.  May be zero on some virtualized environments.
    int speed_mhz = 0;

    /// Cumulative timing breakdown for this core.
    cpu_times times;
};

/// Process resource usage snapshot (mirrors POSIX getrusage).
struct resource_usage {
    /// User-mode CPU time in microseconds.
    std::uint64_t user_time = 0;

    /// Kernel-mode CPU time in microseconds.
    std::uint64_t system_time = 0;

    /// Peak resident set size in kilobytes.
    std::uint64_t max_rss = 0;

    /// Page faults serviced without I/O (minor faults).
    std::uint64_t minor_faults = 0;

    /// Page faults requiring disk I/O (major faults).
    std::uint64_t major_faults = 0;

    /// Context switches initiated by the process yielding the CPU.
    std::uint64_t voluntary_context_switches = 0;

    /// Context switches forced by the scheduler.
    std::uint64_t involuntary_context_switches = 0;
};

/// Operating system identification.
struct uname_info {
    /// OS name (e.g. "Linux", "Darwin", "Windows_NT").
    std::string sysname;

    /// OS release version string.
    std::string release;

    /// Detailed version/build string.
    std::string version;

    /// Hardware architecture (e.g. "x86_64", "aarch64").
    std::string machine;
};

/// Query system memory information.
memory_info memory();

/// Query the resident set size of the current process (in bytes).
result<std::size_t> resident_memory();

/// Query detailed resource usage for the current process.
result<resource_usage> resources();

/// Query per-core CPU information.
result<std::vector<cpu_info>> cpus();

/// Return the number of CPUs available to the process.
unsigned int parallelism();

/// Query OS identification strings.
result<uname_info> uname();

/// Query the system hostname.
result<std::string> hostname();

/// Query the system uptime in seconds.
result<double> uptime();

/// Query the current user's home directory.
result<std::string> home_directory();

/// Query the system temporary directory.
result<std::string> temp_directory();

/// Get the scheduling priority of a process.
/// @param pid  Process ID (0 = current process).
result<int> priority(int pid = 0);

/// Set the scheduling priority of a process.
/// @param value  Nice value; higher = lower priority.
/// @param pid    Process ID (0 = current process).
error set_priority(int value, int pid = 0);

}  // namespace kota::sys
