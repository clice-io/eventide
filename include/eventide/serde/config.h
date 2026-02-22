#pragma once

#include <string>
#include <string_view>

#include "spelling.h"

namespace eventide::serde::config {

struct runtime_config {
    using rename_transform_fn = std::string (*)(bool is_serialize, std::string_view value);
    rename_transform_fn field_rename = nullptr;
    rename_transform_fn enum_rename = nullptr;
};

namespace detail {

template <typename Policy>
std::string apply_policy_rename(bool is_serialize, std::string_view value) {
    return spelling::apply_rename_policy<Policy>(is_serialize, value);
}

}  // namespace detail

inline thread_local runtime_config runtime{};

const inline runtime_config& get() {
    return runtime;
}

inline void set(runtime_config cfg) {
    runtime = cfg;
}

inline void reset() {
    runtime = {};
}

inline bool field_rename_enabled() {
    return get().field_rename != nullptr;
}

inline bool enum_rename_enabled() {
    return get().enum_rename != nullptr;
}

template <typename Policy>
void set_field_rename_policy() {
    auto cfg = get();
    cfg.field_rename = &detail::apply_policy_rename<Policy>;
    set(cfg);
}

template <typename Policy>
void set_enum_rename_policy() {
    auto cfg = get();
    cfg.enum_rename = &detail::apply_policy_rename<Policy>;
    set(cfg);
}

inline std::string_view apply_field_rename(bool is_serialize,
                                           std::string_view value,
                                           std::string& scratch) {
    auto fn = get().field_rename;
    if(fn == nullptr) {
        return value;
    }
    scratch = fn(is_serialize, value);
    return scratch;
}

inline std::string_view apply_enum_rename(bool is_serialize,
                                          std::string_view value,
                                          std::string& scratch) {
    auto fn = get().enum_rename;
    if(fn == nullptr) {
        return value;
    }
    scratch = fn(is_serialize, value);
    return scratch;
}

class scoped_config {
public:
    explicit scoped_config(runtime_config cfg) : prev(get()) {
        set(cfg);
    }

    ~scoped_config() {
        set(prev);
    }

    scoped_config(const scoped_config&) = delete;
    scoped_config& operator=(const scoped_config&) = delete;

private:
    runtime_config prev;
};

}  // namespace eventide::serde::config
