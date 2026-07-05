#if __has_include(<flatbuffers/flatbuffers.h>)

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/meta/attrs.h"
#include "kota/codec/fbs/fbs.h"
#include "flatbuffers/flatbuffers.h"

namespace kota::codec {

namespace {

using fbs::from_flatbuffer;
using fbs::map_view;
using fbs::table_view;
using fbs::to_flatbuffer;

// A minimal map with deterministic, deliberately unsorted iteration order.
// Entries are plain pairs, so the container classifies as a map via the
// pair-like entry protocol.
template <typename K, typename V>
struct scrambled_map {
    using key_type = K;
    using mapped_type = V;

    std::vector<std::pair<K, V>> entries;

    auto begin() const {
        return entries.begin();
    }

    auto end() const {
        return entries.end();
    }

    auto begin() {
        return entries.begin();
    }

    auto end() {
        return entries.end();
    }

    void clear() {
        entries.clear();
    }

    void insert_or_assign(K key, V value) {
        for(auto& [k, v]: entries) {
            if(k == key) {
                v = std::move(value);
                return;
            }
        }
        entries.emplace_back(std::move(key), std::move(value));
    }
};

// Mimics llvm::StringMap: entries expose getKey()/getValue() instead of
// first/second, key storage is owned by the container, and key_type is a
// non-owning const char* the decoder must not decode into directly.
template <typename V>
struct mock_string_map {
    struct entry_type {
        std::string key_storage;
        V value_storage;

        std::string_view getKey() const {
            return key_storage;
        }

        const V& getValue() const {
            return value_storage;
        }

        V& getValue() {
            return value_storage;
        }
    };

    using key_type = const char*;
    using mapped_type = V;

    std::vector<entry_type> entries;

    auto begin() const {
        return entries.begin();
    }

    auto end() const {
        return entries.end();
    }

    auto begin() {
        return entries.begin();
    }

    auto end() {
        return entries.end();
    }

    void clear() {
        entries.clear();
    }

    void insert_or_assign(std::string_view key, V value) {
        for(auto& e: entries) {
            if(e.key_storage == key) {
                e.value_storage = std::move(value);
                return;
            }
        }
        entries.push_back(entry_type{std::string(key), std::move(value)});
    }

    const V* find(std::string_view key) const {
        for(const auto& e: entries) {
            if(e.key_storage == key) {
                return &e.value_storage;
            }
        }
        return nullptr;
    }
};

struct int_key_holder {
    scrambled_map<std::uint32_t, std::string> table;
};

struct u64_key_holder {
    scrambled_map<std::uint64_t, std::int32_t> table;
};

struct span_key {
    std::uint32_t begin;
    std::uint32_t end;

    constexpr auto operator<=>(const span_key&) const = default;
};

struct span_key_holder {
    scrambled_map<span_key, std::int32_t> table;
};

struct string_map_holder {
    mock_string_map<std::int32_t> table;
};

// Entries that are plain two-field aggregates: no first/second, no tuple
// protocol, no getKey/getValue — only destructurable via structured bindings.
struct aggregate_entry_map {
    struct entry_type {
        std::uint32_t id;
        std::string label;
    };

    using key_type = std::uint32_t;
    using mapped_type = std::string;

    std::vector<entry_type> entries;

    auto begin() const {
        return entries.begin();
    }

    auto end() const {
        return entries.end();
    }

    auto begin() {
        return entries.begin();
    }

    auto end() {
        return entries.end();
    }

    void clear() {
        entries.clear();
    }

    void insert_or_assign(std::uint32_t key, std::string value) {
        for(auto& e: entries) {
            if(e.id == key) {
                e.label = std::move(value);
                return;
            }
        }
        entries.push_back(entry_type{key, std::move(value)});
    }
};

struct aggregate_entry_holder {
    aggregate_entry_map table;
};

TEST_SUITE(serde_flatbuffers_map_protocol) {

// Multi-digit integer keys used to be sorted by their decimal string
// representation ("10" < "9"), breaking map_view's numeric binary search.
TEST_CASE(integer_keys_sort_numerically_for_binary_search) {
    int_key_holder input;
    for(std::uint32_t key: {17U, 3U, 100U, 9U, 25U, 1U, 42U, 10U, 7U, 88U, 2U, 56U}) {
        input.table.insert_or_assign(key, "v" + std::to_string(key));
    }

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<int_key_holder>::from_bytes(*encoded);
    ASSERT_TRUE(root.valid());

    auto view = root[&int_key_holder::table];
    ASSERT_TRUE(view.valid());
    ASSERT_EQ(view.size(), input.table.entries.size());

    for(const auto& [key, value]: input.table.entries) {
        ASSERT_TRUE(view.contains(key));
        EXPECT_EQ(view[key], std::string_view(value));
    }
    EXPECT_FALSE(view.contains(11U));

    // wire order must follow numeric key order
    std::uint32_t last = 0;
    for(std::size_t i = 0; i < view.size(); ++i) {
        auto key = view.at(i).template get<0>();
        if(i > 0) {
            EXPECT_TRUE(last < key);
        }
        last = key;
    }
}

TEST_CASE(u64_keys_survive_round_trip_and_lookup) {
    u64_key_holder input;
    const std::uint64_t keys[] = {
        0xdeadbeefcafebabeULL,
        7ULL,
        0x10000000000ULL,
        0xffffffffffffffffULL,
        1000ULL,
    };
    std::int32_t v = 1;
    for(auto key: keys) {
        input.table.insert_or_assign(key, v++);
    }

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<u64_key_holder>::from_bytes(*encoded);
    auto view = root[&u64_key_holder::table];
    ASSERT_EQ(view.size(), 5U);
    for(const auto& [key, value]: input.table.entries) {
        ASSERT_TRUE(view.contains(key));
        EXPECT_EQ(view[key], value);
    }

    u64_key_holder decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(decoded.table.entries.size(), 5U);
    for(const auto& [key, value]: input.table.entries) {
        bool found = false;
        for(const auto& [dk, dv]: decoded.table.entries) {
            if(dk == key) {
                EXPECT_EQ(dv, value);
                found = true;
            }
        }
        EXPECT_TRUE(found);
    }
}

// Struct keys previously failed to compile (the encoder stringified keys for
// sorting). They now sort by the key's own ordering, which map_view reuses.
TEST_CASE(inline_struct_keys_sort_and_lookup) {
    span_key_holder input;
    input.table.insert_or_assign(span_key{30, 40}, 3);
    input.table.insert_or_assign(span_key{10, 20}, 1);
    input.table.insert_or_assign(span_key{10, 15}, 0);
    input.table.insert_or_assign(span_key{50, 60}, 5);
    input.table.insert_or_assign(span_key{20, 25}, 2);

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<span_key_holder>::from_bytes(*encoded);
    auto view = root[&span_key_holder::table];
    ASSERT_EQ(view.size(), 5U);

    for(const auto& [key, value]: input.table.entries) {
        ASSERT_TRUE(view.contains(key));
        EXPECT_EQ(view[key], value);
    }
    EXPECT_FALSE(view.contains(span_key{10, 21}));

    span_key last{};
    for(std::size_t i = 0; i < view.size(); ++i) {
        auto key = view.at(i).template get<0>();
        if(i > 0) {
            EXPECT_TRUE(last < key);
        }
        last = key;
    }

    span_key_holder decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.table.entries.size(), 5U);
    for(const auto& [key, value]: input.table.entries) {
        bool found = false;
        for(const auto& [dk, dv]: decoded.table.entries) {
            if(dk == key) {
                EXPECT_EQ(dv, value);
                found = true;
            }
        }
        EXPECT_TRUE(found);
    }
}

// llvm::StringMap-shaped containers: getKey()/getValue() entries, view-typed
// keys, container-owned key storage.
TEST_CASE(keyed_entry_container_encodes_as_map) {
    string_map_holder input;
    input.table.insert_or_assign("banana", 2);
    input.table.insert_or_assign("apple", 1);
    input.table.insert_or_assign("cherry", 3);

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<string_map_holder>::from_bytes(*encoded);
    auto view = root[&string_map_holder::table];
    ASSERT_TRUE(view.valid());
    ASSERT_EQ(view.size(), 3U);

    EXPECT_EQ(view[std::string_view("apple")], 1);
    EXPECT_EQ(view[std::string_view("banana")], 2);
    EXPECT_EQ(view[std::string_view("cherry")], 3);
    EXPECT_FALSE(view.contains(std::string_view("durian")));

    // wire order is lexicographic regardless of insertion order
    EXPECT_EQ(view.at(0).template get<0>(), "apple");
    EXPECT_EQ(view.at(1).template get<0>(), "banana");
    EXPECT_EQ(view.at(2).template get<0>(), "cherry");

    string_map_holder decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(decoded.table.entries.size(), 3U);
    const auto* apple = decoded.table.find("apple");
    ASSERT_TRUE(apple != nullptr);
    EXPECT_EQ(*apple, 1);
    const auto* cherry = decoded.table.find("cherry");
    ASSERT_TRUE(cherry != nullptr);
    EXPECT_EQ(*cherry, 3);
}

TEST_CASE(aggregate_entry_container_encodes_as_map) {
    aggregate_entry_holder input;
    input.table.insert_or_assign(12U, "twelve");
    input.table.insert_or_assign(3U, "three");
    input.table.insert_or_assign(101U, "hundred-one");

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    auto root = table_view<aggregate_entry_holder>::from_bytes(*encoded);
    auto view = root[&aggregate_entry_holder::table];
    ASSERT_TRUE(view.valid());
    ASSERT_EQ(view.size(), 3U);
    EXPECT_EQ(view[3U], std::string_view("three"));
    EXPECT_EQ(view[12U], std::string_view("twelve"));
    EXPECT_EQ(view[101U], std::string_view("hundred-one"));

    aggregate_entry_holder decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(decoded.table.entries.size(), 3U);
}

TEST_CASE(std_map_round_trip_still_works) {
    std::map<std::string, std::vector<std::int32_t>> input{
        {"a", {1, 2}},
        {"b", {}},
        {"c", {3}},
    };

    auto encoded = to_flatbuffer(input);
    ASSERT_TRUE(encoded.has_value());

    std::map<std::string, std::vector<std::int32_t>> decoded;
    auto result = from_flatbuffer(std::span<const std::uint8_t>(*encoded), decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(decoded == input);
}

};  // TEST_SUITE(serde_flatbuffers_map_protocol)

}  // namespace

}  // namespace kota::codec

#endif
