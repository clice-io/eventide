#include "kota/async/io/system.h"

#include "../libuv.h"

namespace kota::sys {

memory_info memory() {
    memory_info info;
    info.total = ::uv_get_total_memory();
    info.free = ::uv_get_free_memory();
    info.available = ::uv_get_available_memory();
    info.constrained = ::uv_get_constrained_memory();
    return info;
}

result<std::size_t> resident_memory() {
    std::size_t rss = 0;
    if(auto err = uv::resident_set_memory(rss)) {
        return outcome_error(err);
    }
    return rss;
}

result<resource_usage> resources() {
    uv_rusage_t ru{};
    if(auto err = uv::getrusage(ru)) {
        return outcome_error(err);
    }

    resource_usage usage;
    usage.user_time =
        static_cast<std::uint64_t>(ru.ru_utime.tv_sec) * 1'000'000 + ru.ru_utime.tv_usec;
    usage.system_time =
        static_cast<std::uint64_t>(ru.ru_stime.tv_sec) * 1'000'000 + ru.ru_stime.tv_usec;
    usage.max_rss = ru.ru_maxrss;
    usage.minor_faults = ru.ru_minflt;
    usage.major_faults = ru.ru_majflt;
    usage.voluntary_context_switches = ru.ru_nvcsw;
    usage.involuntary_context_switches = ru.ru_nivcsw;
    return usage;
}

result<std::vector<cpu_info>> cpus() {
    uv_cpu_info_t* infos = nullptr;
    int count = 0;
    if(auto err = uv::cpu_info(infos, count)) {
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
    uv::free_cpu_info(infos, count);
    return result;
}

unsigned int parallelism() {
    return ::uv_available_parallelism();
}

result<uname_info> uname() {
    uv_utsname_t buf{};
    if(auto err = uv::os_uname(buf)) {
        return outcome_error(err);
    }
    return uname_info{buf.sysname, buf.release, buf.version, buf.machine};
}

result<std::string> hostname() {
    char buf[256]{};
    std::size_t size = sizeof(buf);
    if(auto err = uv::os_gethostname(buf, size)) {
        return outcome_error(err);
    }
    return std::string(buf, size);
}

result<double> uptime() {
    double value = 0;
    if(auto err = uv::uptime(value)) {
        return outcome_error(err);
    }
    return value;
}

result<std::string> home_directory() {
    char buf[1024]{};
    std::size_t size = sizeof(buf);
    if(auto err = uv::os_homedir(buf, size)) {
        return outcome_error(err);
    }
    return std::string(buf, size);
}

result<std::string> temp_directory() {
    char buf[1024]{};
    std::size_t size = sizeof(buf);
    if(auto err = uv::os_tmpdir(buf, size)) {
        return outcome_error(err);
    }
    return std::string(buf, size);
}

result<int> priority(int pid) {
    int value = 0;
    if(auto err = uv::os_getpriority(static_cast<uv_pid_t>(pid), value)) {
        return outcome_error(err);
    }
    return value;
}

error set_priority(int value, int pid) {
    return uv::os_setpriority(static_cast<uv_pid_t>(pid), value);
}

}  // namespace kota::sys
