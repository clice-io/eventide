#include "kota/zest/zest.h"
#include "kota/async/io/system.h"

namespace kota {

TEST_SUITE(system_info) {

TEST_CASE(memory_info_sane) {
    auto info = get_memory_info();
    EXPECT_TRUE(info.total > 0);
    EXPECT_TRUE(info.free > 0);
    EXPECT_TRUE(info.available > 0);
    EXPECT_TRUE(info.free <= info.total);
    EXPECT_TRUE(info.available <= info.total);
}

TEST_CASE(resident_memory) {
    auto rss = get_resident_memory();
    ASSERT_TRUE(rss.has_value());
    EXPECT_TRUE(*rss > 0);
}

TEST_CASE(resource_usage_times) {
    auto ru = get_resource_usage();
    ASSERT_TRUE(ru.has_value());
    // The test process must have consumed some user CPU time.
    EXPECT_TRUE(ru->utime_us > 0);
    EXPECT_TRUE(ru->max_rss_kb > 0);
}

TEST_CASE(cpu_info_populated) {
    auto cpus = get_cpu_info();
    ASSERT_TRUE(cpus.has_value());
    EXPECT_TRUE(!cpus->empty());
    // speed_mhz may be 0 on some virtualized environments.
    for(auto& cpu: *cpus) {
        EXPECT_TRUE(!cpu.model.empty());
        EXPECT_TRUE(cpu.speed_mhz >= 0);
    }
}

TEST_CASE(available_parallelism_positive) {
    EXPECT_TRUE(available_parallelism() >= 1);
}

TEST_CASE(uname_populated) {
    auto name = get_uname();
    ASSERT_TRUE(name.has_value());
    EXPECT_TRUE(!name->sysname.empty());
    EXPECT_TRUE(!name->machine.empty());
}

TEST_CASE(hostname_nonempty) {
    auto host = get_hostname();
    ASSERT_TRUE(host.has_value());
    EXPECT_TRUE(!host->empty());
}

TEST_CASE(uptime_positive) {
    auto up = get_uptime();
    ASSERT_TRUE(up.has_value());
    EXPECT_TRUE(*up > 0);
}

TEST_CASE(homedir_nonempty) {
    auto home = get_homedir();
    ASSERT_TRUE(home.has_value());
    EXPECT_TRUE(!home->empty());
}

TEST_CASE(tmpdir_nonempty) {
    auto tmp = get_tmpdir();
    ASSERT_TRUE(tmp.has_value());
    EXPECT_TRUE(!tmp->empty());
}

TEST_CASE(get_set_priority) {
    auto orig = get_priority();
    ASSERT_TRUE(orig.has_value());

    // Lower priority (higher nice value), then restore.
    auto err = set_priority(*orig + 1);
    EXPECT_TRUE(!err.has_error());

    auto changed = get_priority();
    ASSERT_TRUE(changed.has_value());
    EXPECT_EQ(*changed, *orig + 1);

    set_priority(*orig);
}

};  // TEST_SUITE(system_info)

}  // namespace kota
