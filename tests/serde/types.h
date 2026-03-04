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
