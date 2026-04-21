#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "kota/async/vocab/error.h"

namespace kota {

/// System memory information (in bytes).
struct memory_info {
    /// Total physical memory.
    std::uint64_t total = 0;

    /// Free physical memory.
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
    std::uint64_t user = 0;
    std::uint64_t nice = 0;
    std::uint64_t sys = 0;
    std::uint64_t idle = 0;
    std::uint64_t irq = 0;
};

/// Information about a single logical CPU core.
struct cpu_info {
    std::string model;
    int speed_mhz = 0;
    cpu_times times;
};

/// Process resource usage snapshot (mirrors POSIX getrusage).
struct resource_usage {
    /// User CPU time in microseconds.
    std::uint64_t utime_us = 0;

    /// System CPU time in microseconds.
    std::uint64_t stime_us = 0;

    /// Peak resident set size in kilobytes.
    std::uint64_t max_rss_kb = 0;

    /// Page faults not requiring I/O.
    std::uint64_t minor_faults = 0;

    /// Page faults requiring I/O.
    std::uint64_t major_faults = 0;

    /// Voluntary context switches.
    std::uint64_t voluntary_context_switches = 0;

    /// Involuntary context switches.
    std::uint64_t involuntary_context_switches = 0;
};

/// Operating system identification.
struct system_uname {
    std::string sysname;
    std::string release;
    std::string version;
    std::string machine;
};

/// Query system memory information.
memory_info get_memory_info();

/// Query the resident set size of the current process (in bytes).
result<std::size_t> get_resident_memory();

/// Query detailed resource usage for the current process.
result<resource_usage> get_resource_usage();

/// Query per-core CPU information.
result<std::vector<cpu_info>> get_cpu_info();

/// Return the number of CPUs available to the process.
unsigned int available_parallelism();

/// Query OS identification strings.
result<system_uname> get_uname();

/// Query the system hostname.
result<std::string> get_hostname();

/// Query the system uptime in seconds.
result<double> get_uptime();

/// Query the current user's home directory.
result<std::string> get_homedir();

/// Query the system temporary directory.
result<std::string> get_tmpdir();

/// Get the scheduling priority of a process.
/// @param pid  Process ID (0 = current process).
result<int> get_priority(int pid = 0);

/// Set the scheduling priority of a process.
/// @param priority  Nice value; higher = lower priority.
/// @param pid       Process ID (0 = current process).
error set_priority(int priority, int pid = 0);

}  // namespace kota
