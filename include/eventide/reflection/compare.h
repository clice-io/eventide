#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "struct.h"
#include "eventide/common/range_format.h"

namespace eventide::refl::detail {

template <typename L, typename R>
concept reflectable_pair = reflectable_class<L> && reflectable_class<R>;

template <typename L, typename R>
concept sequence_range_pair = sequence_range<L> && sequence_range<R>;

template <typename L, typename R>
concept set_range_pair = set_range<L> && set_range<R>;

template <typename L, typename R>
concept map_range_pair = map_range<L> && map_range<R>;

template <typename L, typename R>
concept ordered_set_range_pair = ordered_set_range<L> && ordered_set_range<R>;

template <typename L, typename R>
concept ordered_map_range_pair = ordered_map_range<L> && ordered_map_range<R>;

template <typename L, typename R>
constexpr bool compare_eq(const L& lhs, const R& rhs);

template <typename L, typename R>
constexpr bool compare_lt(const L& lhs, const R& rhs);

template <typename L, typename R, typename ElemEq>
constexpr bool compare_range_eq(const L& lhs, const R& rhs, const ElemEq& elem_eq) {
    if constexpr(std::ranges::sized_range<const L> && std::ranges::sized_range<const R>) {
        if(std::ranges::size(lhs) != std::ranges::size(rhs)) {
            return false;
        }
    }

    auto lit = std::ranges::begin(lhs);
    auto lend = std::ranges::end(lhs);
    auto rit = std::ranges::begin(rhs);
    auto rend = std::ranges::end(rhs);

    while(lit != lend && rit != rend) {
        if(!elem_eq(*lit, *rit)) {
            return false;
        }
        ++lit;
        ++rit;
    }
    return lit == lend && rit == rend;
}

template <typename L, typename R, typename ElemLt>
constexpr bool compare_range_lt(const L& lhs, const R& rhs, const ElemLt& elem_lt) {
    auto lit = std::ranges::begin(lhs);
    auto lend = std::ranges::end(lhs);
    auto rit = std::ranges::begin(rhs);
    auto rend = std::ranges::end(rhs);

    while(lit != lend && rit != rend) {
        if(elem_lt(*lit, *rit)) {
            return true;
        }
        if(elem_lt(*rit, *lit)) {
            return false;
        }
        ++lit;
        ++rit;
    }

    return lit == lend && rit != rend;
}

template <typename L, typename R, typename ElemEq>
constexpr bool compare_unordered_range_eq(const L& lhs, const R& rhs, const ElemEq& elem_eq) {
    if constexpr(std::ranges::sized_range<const L> && std::ranges::sized_range<const R>) {
        if(std::ranges::size(lhs) != std::ranges::size(rhs)) {
            return false;
        }
    }

    using RIter = std::ranges::iterator_t<const R>;
    std::vector<RIter> rhs_iters;
    if constexpr(std::ranges::sized_range<const R>) {
        rhs_iters.reserve(std::ranges::size(rhs));
    }
    for(auto it = std::ranges::begin(rhs); it != std::ranges::end(rhs); ++it) {
        rhs_iters.push_back(it);
    }

    std::vector<bool> used(rhs_iters.size(), false);
    for(const auto& l: lhs) {
        bool matched = false;
        for(std::size_t i = 0; i < rhs_iters.size(); ++i) {
            if(used[i]) {
                continue;
            }
            if(elem_eq(l, *rhs_iters[i])) {
                used[i] = true;
                matched = true;
                break;
            }
        }
        if(!matched) {
            return false;
        }
    }

    for(bool matched: used) {
        if(!matched) {
            return false;
        }
    }
    return true;
}

template <typename L, typename R>
constexpr bool compare_sequence_eq(const L& lhs, const R& rhs) {
    return compare_range_eq(lhs, rhs, [](const auto& l, const auto& r) {
        return compare_eq(l, r);
    });
}

template <typename L, typename R>
constexpr bool compare_sequence_lt(const L& lhs, const R& rhs) {
    return compare_range_lt(lhs, rhs, [](const auto& l, const auto& r) {
        return compare_lt(l, r);
    });
}

template <typename L, typename R>
constexpr bool compare_map_entry_eq(const L& lhs, const R& rhs) {
    const auto& [lhs_key, lhs_value] = lhs;
    const auto& [rhs_key, rhs_value] = rhs;
    return compare_eq(lhs_key, rhs_key) && compare_eq(lhs_value, rhs_value);
}

template <typename L, typename R>
constexpr bool compare_map_entry_lt(const L& lhs, const R& rhs) {
    const auto& [lhs_key, lhs_value] = lhs;
    const auto& [rhs_key, rhs_value] = rhs;

    if(compare_lt(lhs_key, rhs_key)) {
        return true;
    }
    if(compare_lt(rhs_key, lhs_key)) {
        return false;
    }
    return compare_lt(lhs_value, rhs_value);
}

template <typename L, typename R>
constexpr bool compare_map_eq(const L& lhs, const R& rhs) {
    return compare_range_eq(lhs, rhs, [](const auto& l, const auto& r) {
        return compare_map_entry_eq(l, r);
    });
}

template <typename L, typename R>
constexpr bool compare_map_eq_unordered(const L& lhs, const R& rhs) {
    return compare_unordered_range_eq(lhs, rhs, [](const auto& l, const auto& r) {
        return compare_map_entry_eq(l, r);
    });
}

template <typename L, typename R>
constexpr bool compare_map_lt(const L& lhs, const R& rhs) {
    return compare_range_lt(lhs, rhs, [](const auto& l, const auto& r) {
        return compare_map_entry_lt(l, r);
    });
}

template <typename L, typename R>
constexpr bool compare_set_eq_unordered(const L& lhs, const R& rhs) {
    return compare_unordered_range_eq(lhs, rhs, [](const auto& l, const auto& r) {
        return compare_eq(l, r);
    });
}

template <typename L, typename R>
constexpr bool compare_eq(const L& lhs, const R& rhs) {
    if constexpr(ordered_map_range_pair<L, R>) {
        return compare_map_eq(lhs, rhs);
    } else if constexpr(map_range_pair<L, R>) {
        return compare_map_eq_unordered(lhs, rhs);
    } else if constexpr(ordered_set_range_pair<L, R>) {
        return compare_sequence_eq(lhs, rhs);
    } else if constexpr(set_range_pair<L, R>) {
        return compare_set_eq_unordered(lhs, rhs);
    } else if constexpr(sequence_range_pair<L, R>) {
        return compare_sequence_eq(lhs, rhs);
    } else if constexpr(eq_comparable_with<L, R>) {
        return static_cast<bool>(lhs == rhs);
    } else if constexpr(reflectable_pair<L, R>) {
        constexpr std::size_t lhs_count = reflection<L>::field_count;
        constexpr std::size_t rhs_count = reflection<R>::field_count;

        if constexpr(lhs_count != rhs_count) {
            return false;
        } else if constexpr(lhs_count == 0) {
            return true;
        } else {
            auto lhs_addrs = reflection<L>::field_addrs(lhs);
            auto rhs_addrs = reflection<R>::field_addrs(rhs);
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return (compare_eq(*std::get<Is>(lhs_addrs), *std::get<Is>(rhs_addrs)) && ...);
            }(std::make_index_sequence<lhs_count>{});
        }
    } else {
        static_assert(dependent_false<L>,
                      "refl::eq: operands are not comparable and not reflectable");
        return false;
    }
}

template <typename L, typename R>
constexpr bool compare_ne(const L& lhs, const R& rhs) {
    if constexpr(ordered_map_range_pair<L, R>) {
        return !compare_map_eq(lhs, rhs);
    } else if constexpr(map_range_pair<L, R>) {
        return !compare_map_eq_unordered(lhs, rhs);
    } else if constexpr(ordered_set_range_pair<L, R>) {
        return !compare_sequence_eq(lhs, rhs);
    } else if constexpr(set_range_pair<L, R>) {
        return !compare_set_eq_unordered(lhs, rhs);
    } else if constexpr(sequence_range_pair<L, R>) {
        return !compare_sequence_eq(lhs, rhs);
    } else if constexpr(ne_comparable_with<L, R>) {
        return static_cast<bool>(lhs != rhs);
    } else {
        return !compare_eq(lhs, rhs);
    }
}

template <typename L, typename R>
constexpr bool compare_lt(const L& lhs, const R& rhs) {
    if constexpr(ordered_map_range_pair<L, R>) {
        return compare_map_lt(lhs, rhs);
    } else if constexpr(map_range_pair<L, R>) {
        static_assert(
            dependent_false<L>,
            "refl::lt: unordered/mixed map ranges have no deterministic strict order; " "use refl::eq/refl::ne");
        return false;
    } else if constexpr(ordered_set_range_pair<L, R>) {
        return compare_sequence_lt(lhs, rhs);
    } else if constexpr(set_range_pair<L, R>) {
        static_assert(
            dependent_false<L>,
            "refl::lt: unordered/mixed set ranges have no deterministic strict order; " "use refl::eq/refl::ne");
        return false;
    } else if constexpr(sequence_range_pair<L, R>) {
        return compare_sequence_lt(lhs, rhs);
    } else if constexpr(lt_comparable_with<L, R>) {
        return static_cast<bool>(lhs < rhs);
    } else if constexpr(reflectable_pair<L, R>) {
        constexpr std::size_t lhs_count = reflection<L>::field_count;
        constexpr std::size_t rhs_count = reflection<R>::field_count;
        constexpr std::size_t common_count = lhs_count < rhs_count ? lhs_count : rhs_count;

        auto lhs_addrs = reflection<L>::field_addrs(lhs);
        auto rhs_addrs = reflection<R>::field_addrs(rhs);

        bool result = false;
        bool decided = false;

        if constexpr(common_count > 0) {
            decided = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return (([&] {
                            const auto& l = *std::get<Is>(lhs_addrs);
                            const auto& r = *std::get<Is>(rhs_addrs);
                            if(compare_lt(l, r)) {
                                result = true;
                                return true;
                            }
                            if(compare_lt(r, l)) {
                                result = false;
                                return true;
                            }
                            return false;
                        }()) ||
                        ...);
            }(std::make_index_sequence<common_count>{});
        }

        if(decided) {
            return result;
        }
        if constexpr(lhs_count != rhs_count) {
            return lhs_count < rhs_count;
        }
        return false;
    } else {
        static_assert(dependent_false<L>,
                      "refl::lt: operands are not comparable and not reflectable");
        return false;
    }
}

template <typename L, typename R>
constexpr bool compare_le(const L& lhs, const R& rhs) {
    // For a strict-weak-order comparator (<), `lhs <= rhs` is equivalent to `!(rhs < lhs)`.
    // This is also equivalent to `(lhs < rhs) || (lhs == rhs)`.
    if constexpr(ordered_map_range_pair<L, R>) {
        return !compare_map_lt(rhs, lhs);
    } else if constexpr(map_range_pair<L, R>) {
        static_assert(dependent_false<L>,
                      "refl::le: unordered/mixed map ranges have no deterministic order");
        return false;
    } else if constexpr(ordered_set_range_pair<L, R>) {
        return !compare_sequence_lt(rhs, lhs);
    } else if constexpr(set_range_pair<L, R>) {
        static_assert(dependent_false<L>,
                      "refl::le: unordered/mixed set ranges have no deterministic order");
        return false;
    } else if constexpr(sequence_range_pair<L, R>) {
        return !compare_sequence_lt(rhs, lhs);
    } else if constexpr(le_comparable_with<L, R>) {
        return static_cast<bool>(lhs <= rhs);
    } else {
        return !compare_lt(rhs, lhs);
    }
}

template <typename L, typename R>
constexpr bool compare_gt(const L& lhs, const R& rhs) {
    if constexpr(ordered_map_range_pair<L, R>) {
        return compare_map_lt(rhs, lhs);
    } else if constexpr(map_range_pair<L, R>) {
        static_assert(dependent_false<L>,
                      "refl::gt: unordered/mixed map ranges have no deterministic order");
        return false;
    } else if constexpr(ordered_set_range_pair<L, R>) {
        return compare_sequence_lt(rhs, lhs);
    } else if constexpr(set_range_pair<L, R>) {
        static_assert(dependent_false<L>,
                      "refl::gt: unordered/mixed set ranges have no deterministic order");
        return false;
    } else if constexpr(sequence_range_pair<L, R>) {
        return compare_sequence_lt(rhs, lhs);
    } else if constexpr(gt_comparable_with<L, R>) {
        return static_cast<bool>(lhs > rhs);
    } else {
        return compare_lt(rhs, lhs);
    }
}

template <typename L, typename R>
constexpr bool compare_ge(const L& lhs, const R& rhs) {
    // Symmetric to <= : `lhs >= rhs` is `!(lhs < rhs)` under strict-weak-order semantics.
    if constexpr(ordered_map_range_pair<L, R>) {
        return !compare_map_lt(lhs, rhs);
    } else if constexpr(map_range_pair<L, R>) {
        static_assert(dependent_false<L>,
                      "refl::ge: unordered/mixed map ranges have no deterministic order");
        return false;
    } else if constexpr(ordered_set_range_pair<L, R>) {
        return !compare_sequence_lt(lhs, rhs);
    } else if constexpr(set_range_pair<L, R>) {
        static_assert(dependent_false<L>,
                      "refl::ge: unordered/mixed set ranges have no deterministic order");
        return false;
    } else if constexpr(sequence_range_pair<L, R>) {
        return !compare_sequence_lt(lhs, rhs);
    } else if constexpr(ge_comparable_with<L, R>) {
        return static_cast<bool>(lhs >= rhs);
    } else {
        return !compare_lt(lhs, rhs);
    }
}

}  // namespace eventide::refl::detail

namespace eventide::refl {

struct eq_fn {
    using is_transparent = void;

    template <typename L, typename R>
    constexpr bool operator()(const L& lhs, const R& rhs) const {
        return detail::compare_eq(lhs, rhs);
    }
};

struct ne_fn {
    using is_transparent = void;

    template <typename L, typename R>
    constexpr bool operator()(const L& lhs, const R& rhs) const {
        return detail::compare_ne(lhs, rhs);
    }
};

struct lt_fn {
    using is_transparent = void;

    template <typename L, typename R>
    constexpr bool operator()(const L& lhs, const R& rhs) const {
        return detail::compare_lt(lhs, rhs);
    }
};

struct le_fn {
    using is_transparent = void;

    template <typename L, typename R>
    constexpr bool operator()(const L& lhs, const R& rhs) const {
        return detail::compare_le(lhs, rhs);
    }
};

struct gt_fn {
    using is_transparent = void;

    template <typename L, typename R>
    constexpr bool operator()(const L& lhs, const R& rhs) const {
        return detail::compare_gt(lhs, rhs);
    }
};

struct ge_fn {
    using is_transparent = void;

    template <typename L, typename R>
    constexpr bool operator()(const L& lhs, const R& rhs) const {
        return detail::compare_ge(lhs, rhs);
    }
};

constexpr inline eq_fn eq{};
constexpr inline ne_fn ne{};
constexpr inline lt_fn lt{};
constexpr inline le_fn le{};
constexpr inline gt_fn gt{};
constexpr inline ge_fn ge{};

}  // namespace eventide::refl
