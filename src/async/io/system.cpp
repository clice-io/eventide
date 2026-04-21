#include "kota/async/io/system.h"

#include <uv.h>

namespace kota {

static error from_uv(int status) {
    return status < 0 ? error(status) : error{};
}

memory_info get_memory_info() {
    memory_info info;
    info.total = uv_get_total_memory();
    info.free = uv_get_free_memory();
    info.available = uv_get_available_memory();
    info.constrained = uv_get_constrained_memory();
    return info;
}

result<std::size_t> get_resident_memory() {
    std::size_t rss = 0;
    if(auto err = from_uv(uv_resident_set_memory(&rss))) {
        return outcome_error(err);
    }
    return rss;
}

result<resource_usage> get_resource_usage() {
    uv_rusage_t ru{};
    if(auto err = from_uv(uv_getrusage(&ru))) {
        return outcome_error(err);
    }

    resource_usage usage;
    usage.utime_us =
        static_cast<std::uint64_t>(ru.ru_utime.tv_sec) * 1'000'000 + ru.ru_utime.tv_usec;
    usage.stime_us =
        static_cast<std::uint64_t>(ru.ru_stime.tv_sec) * 1'000'000 + ru.ru_stime.tv_usec;
    usage.max_rss_kb = ru.ru_maxrss;
    usage.minor_faults = ru.ru_minflt;
    usage.major_faults = ru.ru_majflt;
    usage.voluntary_context_switches = ru.ru_nvcsw;
    usage.involuntary_context_switches = ru.ru_nivcsw;
    return usage;
}

result<std::vector<cpu_info>> get_cpu_info() {
    uv_cpu_info_t* infos = nullptr;
    int count = 0;
    if(auto err = from_uv(uv_cpu_info(&infos, &count))) {
        return outcome_error(err);
    }

    std::vector<cpu_info> result;
    result.reserve(static_cast<std::size_t>(count));
    for(int i = 0; i < count; ++i) {
        auto& src = infos[i];
        cpu_info ci;
        ci.model = src.model ? src.model : "";
        ci.speed_mhz = src.speed;
        ci.times.user = src.cpu_times.user;
        ci.times.nice = src.cpu_times.nice;
        ci.times.sys = src.cpu_times.sys;
        ci.times.idle = src.cpu_times.idle;
        ci.times.irq = src.cpu_times.irq;
        result.push_back(std::move(ci));
    }
    uv_free_cpu_info(infos, count);
    return result;
}

unsigned int available_parallelism() {
    return uv_available_parallelism();
}

result<system_uname> get_uname() {
    uv_utsname_t buf{};
    if(auto err = from_uv(uv_os_uname(&buf))) {
        return outcome_error(err);
    }
    return system_uname{buf.sysname, buf.release, buf.version, buf.machine};
}

result<std::string> get_hostname() {
    char buf[256]{};
    std::size_t size = sizeof(buf);
    if(auto err = from_uv(uv_os_gethostname(buf, &size))) {
        return outcome_error(err);
    }
    return std::string(buf, size);
}

result<double> get_uptime() {
    double uptime = 0;
    if(auto err = from_uv(uv_uptime(&uptime))) {
        return outcome_error(err);
    }
    return uptime;
}

result<std::string> get_homedir() {
    char buf[1024]{};
    std::size_t size = sizeof(buf);
    if(auto err = from_uv(uv_os_homedir(buf, &size))) {
        return outcome_error(err);
    }
    return std::string(buf, size);
}

result<std::string> get_tmpdir() {
    char buf[1024]{};
    std::size_t size = sizeof(buf);
    if(auto err = from_uv(uv_os_tmpdir(buf, &size))) {
        return outcome_error(err);
    }
    return std::string(buf, size);
}

result<int> get_priority(int pid) {
    int priority = 0;
    if(auto err = from_uv(uv_os_getpriority(static_cast<uv_pid_t>(pid), &priority))) {
        return outcome_error(err);
    }
    return priority;
}

error set_priority(int priority, int pid) {
    return from_uv(uv_os_setpriority(static_cast<uv_pid_t>(pid), priority));
}

}  // namespace kota
