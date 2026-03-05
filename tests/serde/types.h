#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace eventide::serde::test_types {

struct Basic {
    bool is_valid{};
    std::int32_t i32{};
    double f64{};
    std::string text;

    auto operator==(const Basic&) const -> bool = default;
};

struct Compound {
    std::vector<std::string> string_list;
    std::array<float, 3> fixed_array{};
    std::tuple<int, bool, std::string> heterogeneous_tuple;

    auto operator==(const Compound&) const -> bool = default;
};

struct Nullables {
    std::optional<int> opt_value;
    std::optional<std::string> opt_empty;
    std::unique_ptr<Basic> heap_allocated;

    auto operator==(const Nullables& other) const -> bool {
        if(opt_value != other.opt_value || opt_empty != other.opt_empty) {
            return false;
        }
        if(static_cast<bool>(heap_allocated) != static_cast<bool>(other.heap_allocated)) {
            return false;
        }
        if(!heap_allocated) {
            return true;
        }
        return *heap_allocated == *other.heap_allocated;
    }
};

enum class Role : std::uint8_t {
    Admin = 1,
    User = 2,
    Guest = 3,
};

struct ADTs {
    Role role{};
    std::variant<std::monostate, int, std::string, Basic> multi_variant;

    auto operator==(const ADTs&) const -> bool = default;
};

struct HardMap {
    std::map<int, std::string> int_keyed_map;

    auto operator==(const HardMap&) const -> bool = default;
};

struct TreeNode {
    std::string name;
    std::vector<std::unique_ptr<TreeNode>> children;

    auto operator==(const TreeNode& other) const -> bool {
        if(name != other.name || children.size() != other.children.size()) {
            return false;
        }

        for(std::size_t index = 0; index < children.size(); ++index) {
            const auto& lhs_child = children[index];
            const auto& rhs_child = other.children[index];
            if(static_cast<bool>(lhs_child) != static_cast<bool>(rhs_child)) {
                return false;
            }
            if(lhs_child && *lhs_child != *rhs_child) {
                return false;
            }
        }
        return true;
    }
};

struct UltimateTest {
    Basic basic;
    Compound compound;
    Nullables nullables;
    ADTs adts;
    HardMap hard_map;
    TreeNode root;

    auto operator==(const UltimateTest&) const -> bool = default;
};

struct Scalars {
    bool b{};
    std::int8_t i8{};
    std::int16_t i16{};
    std::int32_t i32{};
    std::int64_t i64{};
    std::uint8_t u8{};
    std::uint16_t u16{};
    std::uint32_t u32{};
    std::uint64_t u64{};
    float f32{};
    double f64{};
    char ch{};
    std::string str;

    auto operator==(const Scalars&) const -> bool = default;
};

struct NestedContainers {
    std::vector<std::vector<int>> nested_vec;
    std::map<std::string, std::vector<int>> map_of_vecs;
    std::optional<std::vector<int>> opt_vec;

    auto operator==(const NestedContainers&) const -> bool = default;
};

struct EmptyContainers {
    std::vector<int> empty_vec;
    std::map<std::string, int> empty_map;
    std::string empty_str;
    std::optional<int> empty_opt;

    auto operator==(const EmptyContainers&) const -> bool = default;
};

inline auto make_scalars() -> Scalars {
    return Scalars{
        .b = true,
        .i8 = -42,
        .i16 = -1000,
        .i32 = -100000,
        .i64 = -9999999999LL,
        .u8 = 200,
        .u16 = 50000,
        .u32 = 3000000000U,
        .u64 = 10000000000ULL,
        .f32 = 3.14F,
        .f64 = 2.718281828,
        .ch = 'Z',
        .str = "hello scalars",
    };
}

inline auto make_nested_containers() -> NestedContainers {
    return NestedContainers{
        .nested_vec = {{1, 2, 3}, {4, 5}, {6}},
        .map_of_vecs = {{"a", {10, 20}}, {"b", {30}}},
        .opt_vec = std::vector<int>{1, 2, 3},
    };
}

inline auto make_empty_containers() -> EmptyContainers {
    return EmptyContainers{
        .empty_vec = {},
        .empty_map = {},
        .empty_str = "",
        .empty_opt = std::nullopt,
    };
}

inline auto make_ultimate() -> UltimateTest {
    UltimateTest out;

    out.basic = Basic{
        .is_valid = true,
        .i32 = -42,
        .f64 = 3.1415926,
        .text = "basic",
    };

    out.compound = Compound{
        .string_list = {"alpha", "beta", "gamma"},
        .fixed_array = {1.5F,    -2.25F, 0.0F   },
        .heterogeneous_tuple = std::tuple<int, bool, std::string>{7,       true,   "tuple"},
    };

    out.nullables = Nullables{
        .opt_value = 17,
        .opt_empty = std::nullopt,
        .heap_allocated = std::make_unique<Basic>(Basic{
            .is_valid = false,
            .i32 = 128,
            .f64 = -9.75,
            .text = "heap",
        }),
    };

    out.adts = ADTs{
        .role = Role::User,
        .multi_variant =
            Basic{
                  .is_valid = true,
                  .i32 = 64,
                  .f64 = 2.5,
                  .text = "variant",
                  },
    };

    out.hard_map = HardMap{
        .int_keyed_map = {{-2, "minus-two"}, {0, "zero"}, {7, "seven"}},
    };

    out.root.name = "root";
    auto child_a = std::make_unique<TreeNode>();
    child_a->name = "child-a";
    auto leaf = std::make_unique<TreeNode>();
    leaf->name = "leaf";
    child_a->children.push_back(std::move(leaf));

    auto child_b = std::make_unique<TreeNode>();
    child_b->name = "child-b";

    out.root.children.push_back(std::move(child_a));
    out.root.children.push_back(std::move(child_b));

    return out;
}

}  // namespace eventide::serde::test_types
