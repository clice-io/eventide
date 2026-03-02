#include <cstddef>
#include <string>

#include "eventide/common/comptime.h"
#include "eventide/zest/zest.h"

namespace eventide::comptime {

namespace {

constexpr auto k_layout_record = [] {
    ComptimeMemoryResource<counting_flag<0>> counter;
    auto* bytes = static_cast<std::byte*>(counter.allocate(1, alignof(std::byte)));
    counter.deallocate(bytes, 1);
    auto* ints = counter.allocate_type<int>(1);
    counter.deallocate_type(ints, 1);
    return counter.gen_record();
}();

constexpr size_t k_expected_layout_size =
    ((1 + alignof(int) - 1) & ~(alignof(int) - 1)) + sizeof(int);
static_assert(k_layout_record.count == k_expected_layout_size);

constexpr auto k_vector_record = [] {
    ComptimeMemoryResource<counting_flag<2>> counter;

    ComptimeVector<int, decltype(counter), 0> ids(counter);
    ids.push_back(11);
    ids.emplace_back(22);
    ids.push_back(33);
    ids.pop_back();
    ids.push_back(44);

    ComptimeVector<std::string, decltype(counter), 1> names(counter);
    names.emplace_back("alpha");
    names.push_back(std::string("beta"));
    names.clear();
    names.push_back("gamma");
    return counter.gen_record();
}();

template <auto record>
struct record_tag {};

using vector_record_tag = record_tag<k_vector_record>;

static_assert(k_vector_record.data[0] == 3);
static_assert(k_vector_record.data[1] == 1);

TEST_SUITE(comptime) {

TEST_CASE(two_phase_record_keeps_vector_sizes) {
    ComptimeMemoryResource<k_vector_record> storage;
    ComptimeVector<int, decltype(storage), 0> ids(storage);

    EXPECT_TRUE(ids.empty());
    EXPECT_EQ(ids.capacity(), 3u);

    ids.push_back(11);
    ids.emplace_back(22);
    ids.push_back(44);

    EXPECT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids.front(), 11);
    EXPECT_EQ(ids.back(), 44);

    auto* begin = ids.begin();
    auto* end = ids.end();
    EXPECT_EQ(static_cast<size_t>(end - begin), ids.size());
    EXPECT_EQ(begin[1], 22);
}

TEST_CASE(vector_interface_matches_expected_semantics) {
    ComptimeMemoryResource<k_vector_record> storage;
    ComptimeVector<int, decltype(storage), 0> ids(storage);

    ids.push_back(1);
    ids.push_back(2);
    ids.push_back(3);

    auto* ptr = ids.data();
    EXPECT_EQ(ptr[0], 1);
    EXPECT_EQ(ptr[2], 3);

    ids.pop_back();
    EXPECT_EQ(ids.size(), 2u);

    ids.clear();
    EXPECT_TRUE(ids.empty());
}

};  // TEST_SUITE(comptime)

}  // namespace

}  // namespace eventide::comptime
