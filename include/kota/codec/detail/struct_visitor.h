#pragma once

#include <cstddef>
#include <expected>
#include <type_traits>
#include <utility>

#include "kota/support/type_list.h"
#include "kota/support/type_traits.h"
#include "kota/meta/attrs.h"
#include "kota/meta/schema.h"

namespace kota::codec::detail {

template <typename Config, bool IsSerialize, typename T, typename Visitor>
auto for_each_field(T&& value, Visitor&& visitor)
    -> std::expected<void, typename std::remove_cvref_t<Visitor>::error_type> {
    using clean_t = std::remove_cvref_t<T>;
    using schema = meta::virtual_schema<clean_t, Config>;
    using slots = typename schema::slots;
    constexpr std::size_t N = kota::type_list_size_v<slots>;
    using E = typename std::remove_cvref_t<Visitor>::error_type;

    // Determine pointer type based on const-ness of T
    constexpr bool is_const = std::is_const_v<std::remove_reference_t<T>>;

    std::expected<void, E> status{};
    bool ok = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return ([&] {
            using slot_t = kota::type_list_element_t<Is, slots>;
            using raw_t = std::remove_cv_t<typename slot_t::raw_type>;
            using attrs_t = typename slot_t::attrs;

            constexpr std::size_t offset = schema::fields[Is].offset;

            // Get field reference with appropriate const-ness
            decltype(auto) field_ref = [&]() -> decltype(auto) {
                if constexpr(is_const) {
                    const auto* base = reinterpret_cast<const std::byte*>(std::addressof(value));
                    return *reinterpret_cast<const raw_t*>(base + offset);
                } else {
                    auto* base = reinterpret_cast<std::byte*>(std::addressof(value));
                    return *reinterpret_cast<raw_t*>(base + offset);
                }
            }();

            // Check skip_if predicate
            if constexpr(kota::tuple_has_spec_v<attrs_t, meta::behavior::skip_if>) {
                using pred =
                    typename kota::tuple_find_spec_t<attrs_t, meta::behavior::skip_if>::predicate;
                if(meta::evaluate_skip_predicate<pred>(field_ref, IsSerialize)) {
                    auto r = visitor.template on_skip<Is, raw_t, attrs_t>(field_ref);
                    if(!r) {
                        status = std::unexpected(r.error());
                        return false;
                    }
                    return true;
                }
            }

            auto r =
                visitor.template on_field<Is, raw_t, attrs_t>(field_ref, schema::fields[Is].name);
            if(!r) {
                status = std::unexpected(r.error());
                return false;
            }
            return true;
        }() && ...);
    }(std::make_index_sequence<N>{});

    if(!ok) {
        return std::unexpected(status.error());
    }
    return {};
}

}  // namespace kota::codec::detail
