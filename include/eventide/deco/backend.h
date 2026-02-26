#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "eventide/deco/decl.h"
#include "eventide/deco/descriptor.h"
#include "eventide/deco/ty.h"
#include "eventide/common/meta.h"

namespace deco::detail {

struct ParsedNamedOption {
    std::span<const std::string_view> prefixes = backend::pfx_none;
    std::string_view prefix;
    std::string_view name;
};

constexpr auto parse_named_option(std::string_view full_name) {
    if(full_name.starts_with("--")) {
        if(full_name.size() <= 2) {
            throw "Option name cannot be only '--'";
        }
        return ParsedNamedOption{backend::pfx_double, "--", full_name.substr(2)};
    }
    if(full_name.starts_with("-")) {
        if(full_name.size() <= 1) {
            throw "Option name cannot be only '-'";
        }
        return ParsedNamedOption{backend::pfx_dash, "-", full_name.substr(1)};
    }
    if(full_name.starts_with("/")) {
        if(full_name.size() <= 1) {
            throw "Option name cannot be only '/'";
        }
        return ParsedNamedOption{backend::pfx_slash_dash, "/", full_name.substr(1)};
    }
    throw "Option name must start with '-', '--', or '/'";
}

template <bool counting, std::size_t N = 0>
class MemPool {
    using pool_type = std::conditional_t<counting, std::vector<char>, std::array<char, N>>;
    pool_type pool{};
    std::size_t offset = 0;

public:
    constexpr explicit MemPool(std::size_t reserve_bytes = 0) {
        if constexpr(counting) {
            pool.reserve(reserve_bytes);
        } else {
            (void)reserve_bytes;
        }
    }

    constexpr std::string_view add(std::string_view str) {
        if constexpr(counting) {
            offset += str.size() + 1;
            return str;
        } else {
            if(offset + str.size() + 1 > N) {
                throw "String pool overflow";
            }
            std::copy(str.begin(), str.end(), pool.begin() + offset);
            pool[offset + str.size()] = '\0';
            std::string_view result(pool.data() + offset, str.size());
            offset += str.size() + 1;
            return result;
        }
    }

    constexpr std::string_view add_replace(std::string_view str, char old_char, char new_char) {
        if constexpr(counting) {
            offset += str.size() + 1;
            return str;
        } else {
            if(offset + str.size() + 1 > N) {
                throw "String pool overflow";
            }
            for(std::size_t i = 0; i < str.size(); ++i) {
                pool[offset + i] = (str[i] == old_char) ? new_char : str[i];
            }
            pool[offset + str.size()] = '\0';
            std::string_view result(pool.data() + offset, str.size());
            offset += str.size() + 1;
            return result;
        }
    }

    constexpr std::string_view add(std::string_view str1, std::string_view str2) {
        if constexpr(counting) {
            offset += str1.size() + str2.size() + 1;
            return str1;
        } else {
            if(offset + str1.size() + str2.size() + 1 > N) {
                throw "String pool overflow";
            }
            std::copy(str1.begin(), str1.end(), pool.begin() + offset);
            std::copy(str2.begin(), str2.end(), pool.begin() + offset + str1.size());
            pool[offset + str1.size() + str2.size()] = '\0';
            std::string_view result(pool.data() + offset, str1.size() + str2.size());
            offset += str1.size() + str2.size() + 1;
            return result;
        }
    }

    constexpr const char* add_c_str(std::string_view str) {
        if constexpr(counting) {
            add(str);
            return "";
        } else {
            return add(str).data();
        }
    }

    constexpr std::size_t size() const {
        return offset;
    }
};

struct BuildStats {
    std::size_t optCount = 0;
    std::size_t strPoolBytes = 0;
    bool hasTrailingPack = false;
};

template <typename RootTy, bool counting, std::size_t OptN = 0, std::size_t StrN = 0>
class OptBuilder {
public:
    using accessor_fn = void* (*)(void*);

private:
    using info_item = backend::OptTable::Info;
    using pool_type =
        std::conditional_t<counting, std::vector<info_item>, std::array<info_item, OptN + 1>>;
    using id_map_type =
        std::conditional_t<counting, std::vector<accessor_fn>, std::array<accessor_fn, OptN + 1>>;
    using category_map_type = std::conditional_t<counting,
                                                 std::vector<const decl::Category*>,
                                                 std::array<const decl::Category*, OptN + 1>>;

    struct config_state {
        decl::ConfigFields cfg{};
        std::size_t level = 0;
    };

    // Keep a dummy at index 0 so item.id can be used as direct index.
    pool_type pool{};
    MemPool<counting, StrN> strPool;
    id_map_type idMap{};
    category_map_type categoryMap{};

    std::size_t offset = 0;
    bool hasInputSlot = false;
    bool hasTrailingSlot = false;
    bool hasTrailingPack = false;
    unsigned inputOptionId = 0;
    accessor_fn trailingAccessor = nullptr;
    const decl::Category* trailingCategory = nullptr;

    constexpr static auto make_default_item(unsigned id) {
        return info_item::unaliased_one(backend::pfx_none,
                                        "",
                                        id,
                                        backend::Option::UnknownClass,
                                        0,
                                        "no help text",
                                        "");
    }

    constexpr auto& item_by_id(unsigned id) {
        return pool[id];
    }

    template <typename ObjTy, std::size_t I>
    constexpr static auto& field_by_path(ObjTy& object) {
        return refl::field_of<I>(object);
    }

    template <typename ObjTy, std::size_t I, std::size_t J, std::size_t... Rest>
    constexpr static auto& field_by_path(ObjTy& object) {
        auto& nested = refl::field_of<I>(object);
        return field_by_path<std::remove_cvref_t<decltype(nested)>, J, Rest...>(nested);
    }

    template <std::size_t... Path>
    static void* field_accessor(void* object) {
        if(object == nullptr) {
            return nullptr;
        }
        auto& field = field_by_path<RootTy, Path...>(*static_cast<RootTy*>(object));
        return static_cast<void*>(&field);
    }

    template <std::size_t... Path>
    constexpr static accessor_fn accessor_from_path(std::index_sequence<Path...>) {
        return &field_accessor<Path...>;
    }

    constexpr auto& new_item(accessor_fn mapped_accessor = nullptr) {
        if constexpr(counting) {
            const auto item_id = static_cast<unsigned>(pool.size());
            pool.push_back(make_default_item(item_id));
            auto& item = pool.back();
            item.id = item_id;
            idMap.push_back(mapped_accessor);
            categoryMap.push_back(nullptr);
            return item;
        } else {
            if(offset + 1 >= pool.size()) {
                throw "Option pool overflow";
            }
            ++offset;
            const auto item_id = static_cast<unsigned>(offset);
            pool[item_id] = make_default_item(item_id);
            pool[item_id].id = item_id;
            idMap[item_id] = mapped_accessor;
            categoryMap[item_id] = nullptr;
            return pool[item_id];
        }
    }

    constexpr void set_category_for_item(unsigned item_id, const decl::Category* category) {
        categoryMap[item_id] = category;
    }

    constexpr static void config_push(std::vector<config_state>& config_stack,
                                      const decl::ConfigFields& cfg,
                                      std::size_t level) {
        config_stack.push_back(config_state{.cfg = cfg, .level = level});
    }

    constexpr static void config_pop_nearest_start(std::vector<config_state>& config_stack) {
        for(std::size_t i = config_stack.size(); i > 0; --i) {
            if(config_stack[i - 1].cfg.type == decl::ConfigFields::Type::Start) {
                config_stack.resize(i - 1);
                return;
            }
        }
        throw "Unmatched config end field";
    }

    constexpr static void config_consume_next(std::vector<config_state>& config_stack,
                                              std::size_t level) {
        config_stack.erase(std::remove_if(config_stack.begin(),
                                          config_stack.end(),
                                          [level](const config_state& item) {
                                              return item.level == level &&
                                                     item.cfg.type ==
                                                         decl::ConfigFields::Type::Next;
                                          }),
                           config_stack.end());
    }

    constexpr static void on_config_field(std::vector<config_state>& config_stack,
                                          const decl::ConfigFields& cfg,
                                          std::size_t level) {
        switch(cfg.type) {
            case decl::ConfigFields::Type::Start: config_push(config_stack, cfg, level); break;
            case decl::ConfigFields::Type::End: config_pop_nearest_start(config_stack); break;
            case decl::ConfigFields::Type::Next: config_push(config_stack, cfg, level); break;
        }
    }

    template <typename OptTy>
    constexpr static void apply_current_config(OptTy& opt,
                                               const std::vector<config_state>& config_stack) {
        for(const auto& cfg_state: config_stack) {
            const auto& cfg = cfg_state.cfg;
            if(cfg.required.is_overridden()) {
                opt.required = cfg.required.get();
            }
            if(cfg.category.is_overridden()) {
                opt.category = cfg.category.get();
            }
            if(cfg.help.is_overridden()) {
                opt.help = cfg.help.get();
            }
            if(cfg.meta_var.is_overridden()) {
                opt.meta_var = cfg.meta_var.get();
            }
        }
    }

    template <typename CfgTy>
    constexpr static CfgTy make_configured_cfg(const std::vector<config_state>& config_stack) {
        static_assert(std::is_base_of_v<decl::CommonOptionFields, CfgTy>);
        CfgTy cfg{};
        apply_current_config(static_cast<decl::CommonOptionFields&>(cfg), config_stack);
        return cfg;
    }

    template <typename CurrentTy, typename OnOption, std::size_t... Path>
    constexpr static bool visit_fields_impl(const CurrentTy& object,
                                            std::vector<config_state>& config_stack,
                                            std::size_t level,
                                            OnOption& on_option) {
        return refl::for_each(object, [&](auto field) {
            using FieldTy = ty::base_ty<typename decltype(field)::type>;
            constexpr auto idx = decltype(field)::index();
            constexpr auto name = decltype(field)::name();
            if constexpr(ty::is_config_field<FieldTy>) {
                on_config_field(config_stack,
                                static_cast<const decl::ConfigFields&>(field.value()),
                                level);
                return true;
            } else if constexpr(ty::deco_option_like<FieldTy>) {
                using CfgTy = ty::cfg_ty_of<FieldTy>;
                const auto cfg = make_configured_cfg<CfgTy>(config_stack);
                const bool keep_going =
                    bool(on_option(field.value(), cfg, name, std::index_sequence<Path..., idx>{}));
                config_consume_next(config_stack, level);
                return keep_going;
            } else if constexpr(refl::reflectable_class<FieldTy>) {
                const bool keep_going =
                    visit_fields_impl<FieldTy, OnOption, Path..., idx>(field.value(),
                                                                       config_stack,
                                                                       level + 1,
                                                                       on_option);
                config_consume_next(config_stack, level);
                return keep_going;
            } else {
                static_assert(eventide::dependent_false<FieldTy>,
                              "Only deco fields or nested option structs are supported.");
                return true;
            }
        });
    }

    constexpr auto& set_common_options(info_item& item, const decl::CommonOptionFields& fields) {
        if(!fields.help.empty()) {
            item.help_text = strPool.add_c_str(fields.help);
        }
        if(!fields.meta_var.empty()) {
            item.meta_var = strPool.add_c_str(fields.meta_var);
        }
        return item;
    }

    constexpr auto& set_named_options(unsigned item_id,
                                      accessor_fn mapped_accessor,
                                      std::string_view field_name,
                                      const decl::NamedOptionFields& fields) {
        const auto category = fields.category.ptr();
        auto& item = item_by_id(item_id);

        auto set_prefixed_name = [this](info_item& target, std::string_view full_name) {
            auto parsed = parse_named_option(full_name);
            target._prefixes = parsed.prefixes;
            target._prefixed_name = strPool.add(parsed.prefix, parsed.name);
        };

        if(fields.names.empty()) {
            auto normalized_name = strPool.add_replace(field_name, '_', '-');
            if(normalized_name.size() == 1) {
                item._prefixes = backend::pfx_dash;
                item._prefixed_name = strPool.add("-", normalized_name);
            } else {
                item._prefixes = backend::pfx_double;
                item._prefixed_name = strPool.add("--", normalized_name);
            }
            set_common_options(item, fields);
            set_category_for_item(item.id, category);
            return item;
        }

        set_prefixed_name(item, fields.names.front());
        set_category_for_item(item.id, category);

        const auto item_snapshot = item;
        for(std::size_t i = 1; i < fields.names.size(); ++i) {
            auto& alias = new_item(mapped_accessor);
            auto alias_id = alias.id;
            alias = item_snapshot;
            alias.id = alias_id;
            set_prefixed_name(alias, fields.names[i]);
            set_common_options(alias, fields);
            set_category_for_item(alias.id, category);
        }

        set_common_options(item_by_id(item_id), fields);
        return item_by_id(item_id);
    }

    constexpr void add_input_option(const decl::CommonOptionFields& cfg,
                                    accessor_fn mapped_accessor) {
        if(hasInputSlot) {
            throw "Only one DecoInput can be declared";
        }
        hasInputSlot = true;
        if(inputOptionId == 0) {
            auto& item = new_item(mapped_accessor);
            item = info_item::input(item.id);
            inputOptionId = item.id;
        }
        idMap[inputOptionId] = mapped_accessor;
        set_common_options(item_by_id(inputOptionId), cfg);
        set_category_for_item(inputOptionId, cfg.category.ptr());
    }

    constexpr void add_trailing_option(const decl::CommonOptionFields& cfg,
                                       accessor_fn mapped_accessor) {
        if(hasTrailingSlot) {
            throw "Only one DecoPack can be declared";
        }
        hasTrailingSlot = true;
        hasTrailingPack = true;
        trailingAccessor = mapped_accessor;
        trailingCategory = cfg.category.ptr();

        // The backend only has one input id slot. If trailing appears first, reserve that slot
        // now so parse_args can still emit a valid input option id.
        if(inputOptionId == 0) {
            auto& item = new_item(mapped_accessor);
            item = info_item::input(item.id);
            inputOptionId = item.id;
            set_common_options(item, cfg);
            set_category_for_item(item.id, cfg.category.ptr());
        }
    }

    constexpr void add_flag_option(const decl::FlagFields& cfg,
                                   accessor_fn mapped_accessor,
                                   std::string_view field_name) {
        auto& item = new_item(mapped_accessor);
        item.kind = backend::Option::FlagClass;
        item.param = 0;
        set_named_options(item.id, mapped_accessor, field_name, cfg);
    }

    constexpr void add_kv_option(const decl::KVFields& cfg,
                                 accessor_fn mapped_accessor,
                                 std::string_view field_name) {
        auto& item = new_item(mapped_accessor);
        item.kind = (cfg.style == decl::KVStyle::Joined) ? backend::Option::JoinedClass
                                                         : backend::Option::SeparateClass;
        item.param = 1;
        set_named_options(item.id, mapped_accessor, field_name, cfg);
    }

    constexpr void add_comma_option(const decl::CommaJoinedFields& cfg,
                                    accessor_fn mapped_accessor,
                                    std::string_view field_name) {
        auto& item = new_item(mapped_accessor);
        item.kind = backend::Option::CommaJoinedClass;
        item.param = 1;
        set_named_options(item.id, mapped_accessor, field_name, cfg);
    }

    constexpr void add_multi_option(const decl::MultiFields& cfg,
                                    accessor_fn mapped_accessor,
                                    std::string_view field_name) {
        if(cfg.arg_num == 0) {
            throw "DecoMulti arg_num must be greater than 0";
        }
        if(cfg.arg_num > std::numeric_limits<unsigned char>::max()) {
            throw "DecoMulti arg_num exceeds backend param capacity";
        }
        auto& item = new_item(mapped_accessor);
        item.kind = backend::Option::MultiArgClass;
        item.param = static_cast<unsigned char>(cfg.arg_num);
        set_named_options(item.id, mapped_accessor, field_name, cfg);
    }

    template <typename DecoTy>
    constexpr static bool is_field_present(const DecoTy& field) {
        return field.value.has_value();
    }

public:
    constexpr static unsigned unknown_option_id = 1;

    constexpr bool is_unknown_option_id(backend::OptSpecifier id) const {
        return id.id() == 0 || id.id() == unknown_option_id;
    }

    constexpr bool has_input_option() const {
        return hasInputSlot;
    }

    constexpr bool has_trailing_option() const {
        return hasTrailingSlot;
    }

    constexpr explicit OptBuilder(std::size_t reserve_bytes = 0) : strPool(reserve_bytes) {
        if constexpr(counting) {
            pool.reserve(16);
            idMap.reserve(16);
            categoryMap.reserve(16);
        } else {
            idMap.fill(nullptr);
            categoryMap.fill(nullptr);
        }

        // Dummy item: keeps id and index aligned (id 0 => index 0).
        if constexpr(counting) {
            pool.push_back(make_default_item(0));
            idMap.push_back(nullptr);
            categoryMap.push_back(nullptr);
        } else {
            pool[0] = make_default_item(0);
            idMap[0] = nullptr;
            categoryMap[0] = nullptr;
        }

        auto& unknown = new_item(nullptr);
        unknown = info_item::unknown(unknown.id);
    }

    constexpr explicit OptBuilder(std::in_place_t, std::size_t reserve_bytes = 0) :
        OptBuilder(reserve_bytes) {
        build();
    }

    template <typename OnOption>
    constexpr bool visit_fields(const RootTy& object, OnOption&& on_option) const {
        std::vector<config_state> config_stack;
        return visit_fields_impl<RootTy>(object, config_stack, 0, on_option);
    }

    constexpr void build() {
        static_assert(refl::reflectable_class<RootTy>,
                      "OptBuilder root type must be a reflectable struct");
        auto object = RootTy{};
        auto on_option =
            [this](const auto& field, const auto& cfg, std::string_view field_name, auto path) {
                using FieldTy = std::remove_cvref_t<decltype(field)>;
                const auto mapped_accessor = accessor_from_path(path);
                using CfgTy = ty::cfg_ty_of<FieldTy>;
                if constexpr(CfgTy::deco_field_ty == decl::DecoType::Input) {
                    add_input_option(cfg, mapped_accessor);
                } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::TrailingInput) {
                    add_trailing_option(cfg, mapped_accessor);
                } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::Flag) {
                    add_flag_option(cfg, mapped_accessor, field_name);
                } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::KV) {
                    add_kv_option(cfg, mapped_accessor, field_name);
                } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::CommaJoined) {
                    add_comma_option(cfg, mapped_accessor, field_name);
                } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::Multi) {
                    add_multi_option(cfg, mapped_accessor, field_name);
                } else {
                    static_assert(eventide::dependent_false<CfgTy>, "Unsupported deco cfg type.");
                }
                return true;
            };
        visit_fields(object, on_option);
    }

    constexpr std::size_t opt_size() const {
        if constexpr(counting) {
            return pool.size() - 1;
        } else {
            return offset;
        }
    }

    constexpr std::size_t strpool_size() const {
        return strPool.size();
    }

    constexpr auto option_infos() const {
        if constexpr(counting) {
            return std::span<const info_item>(pool.data() + 1, pool.size() - 1);
        } else {
            return std::span<const info_item>(pool.data() + 1, offset);
        }
    }

    constexpr auto id_map() const {
        if constexpr(counting) {
            return std::span<const accessor_fn>(idMap.data(), idMap.size());
        } else {
            return std::span<const accessor_fn>(idMap.data(), offset + 1);
        }
    }

    constexpr auto category_map() const {
        if constexpr(counting) {
            return std::span<const decl::Category* const>(categoryMap.data(), categoryMap.size());
        } else {
            return std::span<const decl::Category* const>(categoryMap.data(), offset + 1);
        }
    }

    constexpr void* field_ptr_of(backend::OptSpecifier opt, RootTy& object) const {
        if(!opt.is_valid()) {
            return nullptr;
        }
        const auto id = opt.id();
        if(id >= id_map().size()) {
            return nullptr;
        }
        auto accessor = idMap[id];
        if(accessor == nullptr) {
            return nullptr;
        }
        return accessor(static_cast<void*>(&object));
    }

    constexpr bool is_input_argument(const backend::ParsedArgument& arg) const {
        if(arg.option_id.id() != inputOptionId) {
            return false;
        }
        return !is_trailing_argument(arg);
    }

    constexpr bool is_trailing_argument(const backend::ParsedArgument& arg) const {
        if(arg.option_id.id() != inputOptionId) {
            return false;
        }
        return arg.get_spelling_view() == "--";
    }

    constexpr void* trailing_ptr_of(RootTy& object) const {
        if(trailingAccessor == nullptr) {
            return nullptr;
        }
        return trailingAccessor(static_cast<void*>(&object));
    }

    constexpr const decl::Category* trailing_category() const {
        return trailingCategory;
    }

    constexpr const decl::Category* category_of(backend::OptSpecifier opt) const {
        if(!opt.is_valid()) {
            return nullptr;
        }
        const auto id = opt.id();
        if(id >= category_map().size()) {
            return nullptr;
        }
        return categoryMap[id];
    }

    auto make_opt_table() const& {
        return backend::OptTable(option_infos(), false, {}, false)
            .set_tablegen_mode(false)
            .set_input_random_index(true)
            .set_dash_dash_parsing(hasTrailingPack)
            .set_dash_dash_as_single_pack(hasTrailingPack)
            .build();
    }

    auto make_opt_table() const&& = delete;

    constexpr BuildStats finish() const {
        static_assert(counting, "finish() is only for counting builders");
        return BuildStats{
            .optCount = pool.size() - 1,
            .strPoolBytes = strPool.size(),
            .hasTrailingPack = hasTrailingPack,
        };
    }
};

template <typename OptDeco>
consteval auto build_stats() {
    OptBuilder<OptDeco, true> counter;
    counter.build();
    return counter.finish();
}

template <typename OptDeco>
struct BuildStorage {
    constexpr inline static BuildStats stats = build_stats<OptDeco>();
    using builder_t = OptBuilder<OptDeco, false, stats.optCount, stats.strPoolBytes>;
    constexpr inline static builder_t value{std::in_place, stats.strPoolBytes};
};

template <typename OptDeco>
constexpr const auto& build_storage() {
    return BuildStorage<OptDeco>::value;
}

}  // namespace deco::detail
