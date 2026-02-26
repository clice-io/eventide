#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

#if __has_include(<yyjson.h>)
#include <yyjson.h>
#else
#error "yyjson.h not found. Enable EVENTIDE_SERDE_ENABLE_YYJSON or add yyjson include paths."
#endif

namespace eventide::serde::json {

class ValueRef;
class ArrayRef;
class ObjectRef;
class Value;
class Array;
class Object;

namespace detail {

struct value_handle {
    yyjson_val* value = nullptr;
    bool is_mutable = false;
};

constexpr auto make_handle(const yyjson_val* value) noexcept -> value_handle {
    return value_handle{
        .value = const_cast<yyjson_val*>(value),
        .is_mutable = false,
    };
}

constexpr auto make_handle(const yyjson_mut_val* value) noexcept -> value_handle {
    return value_handle{
        .value = reinterpret_cast<yyjson_val*>(const_cast<yyjson_mut_val*>(value)),
        .is_mutable = true,
    };
}

constexpr auto as_mutable(value_handle handle) noexcept -> yyjson_mut_val* {
    return reinterpret_cast<yyjson_mut_val*>(handle.value);
}

constexpr auto as_immutable(value_handle handle) noexcept -> const yyjson_val* {
    return handle.value;
}

constexpr auto valid(value_handle handle) noexcept -> bool {
    return handle.value != nullptr;
}

inline auto is_null(value_handle handle) noexcept -> bool {
    if(handle.is_mutable) {
        return yyjson_mut_is_null(as_mutable(handle));
    }
    return yyjson_is_null(handle.value);
}

inline auto is_bool(value_handle handle) noexcept -> bool {
    if(handle.is_mutable) {
        return yyjson_mut_is_bool(as_mutable(handle));
    }
    return yyjson_is_bool(handle.value);
}

inline auto is_int(value_handle handle) noexcept -> bool {
    if(handle.is_mutable) {
        return yyjson_mut_is_int(as_mutable(handle));
    }
    return yyjson_is_int(handle.value);
}

inline auto is_num(value_handle handle) noexcept -> bool {
    if(handle.is_mutable) {
        return yyjson_mut_is_num(as_mutable(handle));
    }
    return yyjson_is_num(handle.value);
}

inline auto is_str(value_handle handle) noexcept -> bool {
    if(handle.is_mutable) {
        return yyjson_mut_is_str(as_mutable(handle));
    }
    return yyjson_is_str(handle.value);
}

inline auto is_arr(value_handle handle) noexcept -> bool {
    if(handle.is_mutable) {
        return yyjson_mut_is_arr(as_mutable(handle));
    }
    return yyjson_is_arr(handle.value);
}

inline auto is_obj(value_handle handle) noexcept -> bool {
    if(handle.is_mutable) {
        return yyjson_mut_is_obj(as_mutable(handle));
    }
    return yyjson_is_obj(handle.value);
}

inline auto arr_size(value_handle handle) noexcept -> std::size_t {
    if(handle.is_mutable) {
        return yyjson_mut_arr_size(as_mutable(handle));
    }
    return yyjson_arr_size(handle.value);
}

inline auto obj_size(value_handle handle) noexcept -> std::size_t {
    if(handle.is_mutable) {
        return yyjson_mut_obj_size(as_mutable(handle));
    }
    return yyjson_obj_size(handle.value);
}

inline auto arr_get(value_handle handle, std::size_t index) noexcept -> value_handle {
    if(handle.is_mutable) {
        return value_handle{
            .value = reinterpret_cast<yyjson_val*>(yyjson_mut_arr_get(as_mutable(handle), index)),
            .is_mutable = true,
        };
    }
    return value_handle{
        .value = yyjson_arr_get(handle.value, index),
        .is_mutable = false,
    };
}

inline auto obj_getn(value_handle handle, std::string_view key) noexcept -> value_handle {
    if(handle.is_mutable) {
        return value_handle{
            .value = reinterpret_cast<yyjson_val*>(
                yyjson_mut_obj_getn(as_mutable(handle), key.data(), key.size())),
            .is_mutable = true,
        };
    }
    return value_handle{
        .value = yyjson_obj_getn(handle.value, key.data(), key.size()),
        .is_mutable = false,
    };
}

inline auto as_bool(value_handle handle) noexcept -> std::optional<bool> {
    if(!is_bool(handle)) {
        return std::nullopt;
    }
    if(handle.is_mutable) {
        return yyjson_mut_get_bool(as_mutable(handle));
    }
    return yyjson_get_bool(handle.value);
}

inline auto as_sint(value_handle handle) noexcept -> std::optional<std::int64_t> {
    if(!is_int(handle)) {
        return std::nullopt;
    }
    if(handle.is_mutable) {
        return yyjson_mut_get_sint(as_mutable(handle));
    }
    return yyjson_get_sint(handle.value);
}

inline auto as_uint(value_handle handle) noexcept -> std::optional<std::uint64_t> {
    if(!is_int(handle)) {
        return std::nullopt;
    }
    if(handle.is_mutable) {
        return yyjson_mut_get_uint(as_mutable(handle));
    }
    return yyjson_get_uint(handle.value);
}

inline auto as_num(value_handle handle) noexcept -> std::optional<double> {
    if(!is_num(handle)) {
        return std::nullopt;
    }
    if(handle.is_mutable) {
        return yyjson_mut_get_num(as_mutable(handle));
    }
    return yyjson_get_num(handle.value);
}

inline auto as_str(value_handle handle) noexcept -> std::optional<std::string_view> {
    if(!is_str(handle)) {
        return std::nullopt;
    }
    if(handle.is_mutable) {
        const char* text = yyjson_mut_get_str(as_mutable(handle));
        return std::string_view(text, yyjson_mut_get_len(as_mutable(handle)));
    }
    const char* text = yyjson_get_str(handle.value);
    return std::string_view(text, yyjson_get_len(handle.value));
}

}  // namespace detail

class RefBase {
public:
    constexpr RefBase() noexcept = default;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return detail::valid(handle_);
    }

    [[nodiscard]] const yyjson_val* raw() const noexcept {
        return detail::as_immutable(handle_);
    }

protected:
    explicit constexpr RefBase(const yyjson_val* value) noexcept :
        handle_(detail::make_handle(value)) {}

    explicit constexpr RefBase(const yyjson_mut_val* value) noexcept :
        handle_(detail::make_handle(value)) {}

    explicit constexpr RefBase(detail::value_handle handle) noexcept : handle_(handle) {}

    [[nodiscard]] constexpr auto handle() const noexcept -> detail::value_handle {
        return handle_;
    }

protected:
    detail::value_handle handle_{};
};

class ValueRef : public RefBase {
public:
    constexpr ValueRef() noexcept = default;

    explicit constexpr ValueRef(const yyjson_val* value) noexcept : RefBase(value) {}

    explicit constexpr ValueRef(const yyjson_mut_val* value) noexcept : RefBase(value) {}

    [[nodiscard]] bool is_null() const noexcept {
        return valid() && detail::is_null(handle_);
    }

    [[nodiscard]] bool is_bool() const noexcept {
        return valid() && detail::is_bool(handle_);
    }

    [[nodiscard]] bool is_int() const noexcept {
        return valid() && detail::is_int(handle_);
    }

    [[nodiscard]] bool is_number() const noexcept {
        return valid() && detail::is_num(handle_);
    }

    [[nodiscard]] bool is_string() const noexcept {
        return valid() && detail::is_str(handle_);
    }

    [[nodiscard]] bool is_array() const noexcept {
        return valid() && detail::is_arr(handle_);
    }

    [[nodiscard]] bool is_object() const noexcept {
        return valid() && detail::is_obj(handle_);
    }

    [[nodiscard]] std::optional<bool> as_bool() const noexcept {
        if(!valid()) {
            return std::nullopt;
        }
        return detail::as_bool(handle_);
    }

    [[nodiscard]] std::optional<std::int64_t> as_int() const noexcept {
        if(!valid()) {
            return std::nullopt;
        }
        return detail::as_sint(handle_);
    }

    [[nodiscard]] std::optional<std::uint64_t> as_uint() const noexcept {
        if(!valid()) {
            return std::nullopt;
        }
        return detail::as_uint(handle_);
    }

    [[nodiscard]] std::optional<double> as_double() const noexcept {
        if(!valid()) {
            return std::nullopt;
        }
        return detail::as_num(handle_);
    }

    [[nodiscard]] std::optional<std::string_view> as_string() const noexcept {
        if(!valid()) {
            return std::nullopt;
        }
        return detail::as_str(handle_);
    }

    [[nodiscard]] std::optional<ArrayRef> as_array() const noexcept;

    [[nodiscard]] std::optional<ObjectRef> as_object() const noexcept;

private:
    explicit constexpr ValueRef(detail::value_handle handle) noexcept : RefBase(handle) {}

private:
    friend class ArrayRef;
    friend class ObjectRef;
};

class ArrayRef : public RefBase {
public:
    constexpr ArrayRef() noexcept = default;

    explicit constexpr ArrayRef(const yyjson_val* value) noexcept : RefBase(value) {}

    explicit constexpr ArrayRef(const yyjson_mut_val* value) noexcept : RefBase(value) {}

    [[nodiscard]] bool valid() const noexcept {
        return RefBase::valid() && detail::is_arr(handle_);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        if(!valid()) {
            return 0;
        }
        return detail::arr_size(handle_);
    }

    [[nodiscard]] ValueRef operator[](std::size_t index) const noexcept {
        if(!valid()) {
            return {};
        }
        return ValueRef(detail::arr_get(handle_, index));
    }

private:
    explicit constexpr ArrayRef(detail::value_handle handle) noexcept : RefBase(handle) {}

private:
    friend class ValueRef;
};

class ObjectRef : public RefBase {
public:
    constexpr ObjectRef() noexcept = default;

    explicit constexpr ObjectRef(const yyjson_val* value) noexcept : RefBase(value) {}

    explicit constexpr ObjectRef(const yyjson_mut_val* value) noexcept : RefBase(value) {}

    [[nodiscard]] bool valid() const noexcept {
        return RefBase::valid() && detail::is_obj(handle_);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        if(!valid()) {
            return 0;
        }
        return detail::obj_size(handle_);
    }

    [[nodiscard]] ValueRef operator[](std::string_view key) const {
        if(!valid()) {
            return {};
        }

        const auto object_size = detail::obj_size(handle_);
        if(object_size < kObjectIndexThreshold) {
            return ValueRef(detail::obj_getn(handle_, key));
        }

        ensure_object_index();
        auto iter = object_index_->find(key);
        if(iter == object_index_->end()) {
            return {};
        }

        return ValueRef(detail::value_handle{
            .value = iter->second,
            .is_mutable = handle_.is_mutable,
        });
    }

private:
    void ensure_object_index() const {
        if(object_index_) {
            return;
        }

        auto index = std::make_unique<std::unordered_map<std::string_view, yyjson_val*>>();
        index->reserve(detail::obj_size(handle_));

        if(handle_.is_mutable) {
            yyjson_mut_val* key = nullptr;
            yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(detail::as_mutable(handle_));
            while((key = yyjson_mut_obj_iter_next(&iter)) != nullptr) {
                yyjson_mut_val* value = yyjson_mut_obj_iter_get_val(key);
                const char* key_text = yyjson_mut_get_str(key);
                if(key_text == nullptr) {
                    continue;
                }
                index->try_emplace(std::string_view(key_text, yyjson_mut_get_len(key)),
                                   reinterpret_cast<yyjson_val*>(value));
            }
        } else {
            yyjson_val* key = nullptr;
            yyjson_obj_iter iter = yyjson_obj_iter_with(handle_.value);
            while((key = yyjson_obj_iter_next(&iter)) != nullptr) {
                yyjson_val* value = yyjson_obj_iter_get_val(key);
                const char* key_text = yyjson_get_str(key);
                if(key_text == nullptr) {
                    continue;
                }
                index->try_emplace(std::string_view(key_text, yyjson_get_len(key)), value);
            }
        }

        object_index_ = std::move(index);
    }

private:
    constexpr static std::size_t kObjectIndexThreshold = 16;

    explicit constexpr ObjectRef(detail::value_handle handle) noexcept : RefBase(handle) {}

private:
    friend class ValueRef;

    mutable std::unique_ptr<std::unordered_map<std::string_view, yyjson_val*>> object_index_;
};

inline auto ValueRef::as_array() const noexcept -> std::optional<ArrayRef> {
    if(!is_array()) {
        return std::nullopt;
    }
    return ArrayRef(handle_);
}

inline auto ValueRef::as_object() const noexcept -> std::optional<ObjectRef> {
    if(!is_object()) {
        return std::nullopt;
    }
    return ObjectRef(handle_);
}

class Value {
public:
    using immutable_doc_ptr = std::shared_ptr<yyjson_doc>;
    using mutable_doc_ptr = std::shared_ptr<yyjson_mut_doc>;
    using doc_owner = std::variant<immutable_doc_ptr, mutable_doc_ptr>;

    enum class error_code : std::uint8_t {
        invalid_state,
        allocation_failed,
        type_mismatch,
    };

    using status_t = std::expected<void, error_code>;

    Value() = default;

    static auto parse(std::string_view json) -> std::expected<Value, yyjson_read_code> {
        yyjson_read_err err{};
        yyjson_doc* raw_doc = yyjson_read_opts(const_cast<char*>(json.data()),
                                               json.size(),
                                               YYJSON_READ_NOFLAG,
                                               nullptr,
                                               &err);
        if(raw_doc == nullptr) {
            return std::unexpected(err.code);
        }

        auto doc = immutable_doc_ptr(raw_doc, yyjson_doc_free);
        return Value(std::move(doc), yyjson_doc_get_root(raw_doc));
    }

    static auto from_immutable_doc(yyjson_doc* raw_doc) -> std::optional<Value> {
        if(raw_doc == nullptr) {
            return std::nullopt;
        }
        auto doc = immutable_doc_ptr(raw_doc, yyjson_doc_free);
        return Value(std::move(doc), yyjson_doc_get_root(raw_doc));
    }

    [[nodiscard]] bool valid() const noexcept {
        return value_ != nullptr;
    }

    [[nodiscard]] bool is_mutable() const noexcept {
        return std::holds_alternative<mutable_doc_ptr>(doc_);
    }

    [[nodiscard]] ValueRef as_ref() const noexcept {
        if(is_mutable()) {
            return ValueRef(mutable_value());
        }
        return ValueRef(readable_value());
    }

    [[nodiscard]] bool is_array() const noexcept {
        return as_ref().is_array();
    }

    [[nodiscard]] bool is_object() const noexcept {
        return as_ref().is_object();
    }

    [[nodiscard]] std::optional<ArrayRef> as_array_ref() const noexcept {
        return as_ref().as_array();
    }

    [[nodiscard]] std::optional<ObjectRef> as_object_ref() const noexcept {
        return as_ref().as_object();
    }

    [[nodiscard]] std::optional<Array> as_array() const;

    [[nodiscard]] std::optional<Object> as_object() const;

    [[nodiscard]] auto to_json_string() const -> std::expected<std::string, yyjson_write_code> {
        if(!valid()) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }

        yyjson_write_err err{};
        size_t len = 0;
        char* out = nullptr;
        if(std::holds_alternative<immutable_doc_ptr>(doc_)) {
            out = yyjson_val_write_opts(readable_value(), YYJSON_WRITE_NOFLAG, nullptr, &len, &err);
        } else {
            out = yyjson_mut_val_write_opts(mutable_value(),
                                            YYJSON_WRITE_NOFLAG,
                                            nullptr,
                                            &len,
                                            &err);
        }
        if(out == nullptr) {
            return std::unexpected(err.code);
        }

        std::string json(out, len);
        std::free(out);
        return json;
    }

    status_t set_null() {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_set_null(*mutable_root)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t set_bool(bool value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_set_bool(*mutable_root, value)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t set_int(std::int64_t value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_set_sint(*mutable_root, value)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t set_uint(std::uint64_t value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_set_uint(*mutable_root, value)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t set_real(double value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_set_real(*mutable_root, value)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t append_int(std::int64_t value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_is_arr(*mutable_root)) {
            return std::unexpected(error_code::type_mismatch);
        }

        auto* doc = mutable_doc();
        if(doc == nullptr) {
            return std::unexpected(error_code::invalid_state);
        }
        if(!yyjson_mut_arr_add_sint(doc, *mutable_root, value)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t append(const Value& value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_is_arr(*mutable_root)) {
            return std::unexpected(error_code::type_mismatch);
        }

        auto copied = copy_subtree_into_this_doc(value);
        if(!copied) {
            return std::unexpected(copied.error());
        }
        if(!yyjson_mut_arr_add_val(*mutable_root, *copied)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t put_int(std::string_view key, std::int64_t value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_is_obj(*mutable_root)) {
            return std::unexpected(error_code::type_mismatch);
        }

        auto* doc = mutable_doc();
        if(doc == nullptr) {
            return std::unexpected(error_code::invalid_state);
        }

        yyjson_mut_val* key_value = yyjson_mut_strncpy(doc, key.data(), key.size());
        yyjson_mut_val* int_value = yyjson_mut_sint(doc, value);
        if(key_value == nullptr || int_value == nullptr) {
            return std::unexpected(error_code::allocation_failed);
        }

        if(!yyjson_mut_obj_put(*mutable_root, key_value, int_value)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    status_t put(std::string_view key, const Value& value) {
        auto mutable_root = mutable_root_value();
        if(!mutable_root) {
            return std::unexpected(mutable_root.error());
        }
        if(!yyjson_mut_is_obj(*mutable_root)) {
            return std::unexpected(error_code::type_mismatch);
        }

        auto* doc = mutable_doc();
        if(doc == nullptr) {
            return std::unexpected(error_code::invalid_state);
        }

        yyjson_mut_val* key_value = yyjson_mut_strncpy(doc, key.data(), key.size());
        if(key_value == nullptr) {
            return std::unexpected(error_code::allocation_failed);
        }

        auto copied = copy_subtree_into_this_doc(value);
        if(!copied) {
            return std::unexpected(copied.error());
        }
        if(!yyjson_mut_obj_put(*mutable_root, key_value, *copied)) {
            return std::unexpected(error_code::allocation_failed);
        }
        return {};
    }

    [[nodiscard]] const yyjson_val* immutable_value() const noexcept {
        return readable_value();
    }

    [[nodiscard]] const yyjson_mut_val* mutable_value() const noexcept {
        if(!is_mutable()) {
            return nullptr;
        }
        return static_cast<const yyjson_mut_val*>(value_);
    }

protected:
    Value(doc_owner doc, const yyjson_val* value) :
        doc_(std::move(doc)), value_(const_cast<void*>(static_cast<const void*>(value))) {}

    [[nodiscard]] const yyjson_val* readable_value() const noexcept {
        if(value_ == nullptr) {
            return nullptr;
        }
        if(std::holds_alternative<immutable_doc_ptr>(doc_)) {
            return static_cast<const yyjson_val*>(value_);
        }
        return reinterpret_cast<const yyjson_val*>(static_cast<const yyjson_mut_val*>(value_));
    }

    [[nodiscard]] yyjson_mut_doc* mutable_doc() noexcept {
        if(!std::holds_alternative<mutable_doc_ptr>(doc_)) {
            return nullptr;
        }
        return std::get<mutable_doc_ptr>(doc_).get();
    }

    status_t ensure_writable() {
        if(!valid()) {
            return std::unexpected(error_code::invalid_state);
        }

        if(std::holds_alternative<immutable_doc_ptr>(doc_)) {
            auto* copied = yyjson_doc_mut_copy(std::get<immutable_doc_ptr>(doc_).get(), nullptr);
            if(copied == nullptr) {
                return std::unexpected(error_code::allocation_failed);
            }
            doc_ = mutable_doc_ptr(copied, yyjson_mut_doc_free);
            value_ = yyjson_mut_doc_get_root(copied);
            return {};
        }

        auto& current = std::get<mutable_doc_ptr>(doc_);
        if(current.use_count() <= 1) {
            return {};
        }

        auto* copied = yyjson_mut_doc_mut_copy(current.get(), nullptr);
        if(copied == nullptr) {
            return std::unexpected(error_code::allocation_failed);
        }
        doc_ = mutable_doc_ptr(copied, yyjson_mut_doc_free);
        value_ = yyjson_mut_doc_get_root(copied);
        return {};
    }

    [[nodiscard]] auto mutable_root_value() -> std::expected<yyjson_mut_val*, error_code> {
        auto writable = ensure_writable();
        if(!writable) {
            return std::unexpected(writable.error());
        }

        auto* root = static_cast<yyjson_mut_val*>(value_);
        if(root == nullptr) {
            return std::unexpected(error_code::invalid_state);
        }
        return root;
    }

    [[nodiscard]] auto copy_subtree_into_this_doc(const Value& value)
        -> std::expected<yyjson_mut_val*, error_code> {
        auto* doc = mutable_doc();
        if(doc == nullptr) {
            return std::unexpected(error_code::invalid_state);
        }

        yyjson_mut_val* copied = nullptr;
        if(value.is_mutable()) {
            copied =
                yyjson_mut_val_mut_copy(doc, const_cast<yyjson_mut_val*>(value.mutable_value()));
        } else {
            copied = yyjson_val_mut_copy(doc, const_cast<yyjson_val*>(value.immutable_value()));
        }

        if(copied == nullptr) {
            return std::unexpected(error_code::allocation_failed);
        }
        return copied;
    }

protected:
    doc_owner doc_{};
    void* value_ = nullptr;
};

class Array : public Value {
public:
    Array() = default;

    static auto parse(std::string_view json) -> std::expected<Array, yyjson_read_code> {
        auto parsed = Value::parse(json);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        auto array = parsed->as_array();
        if(!array) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }
        return std::move(*array);
    }

    static auto from_immutable_doc(yyjson_doc* raw_doc) -> std::optional<Array> {
        auto value = Value::from_immutable_doc(raw_doc);
        if(!value) {
            return std::nullopt;
        }
        return value->as_array();
    }

    [[nodiscard]] ArrayRef as_ref() const noexcept {
        if(is_mutable()) {
            return ArrayRef(mutable_value());
        }
        return ArrayRef(readable_value());
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return as_ref().size();
    }

    status_t append_int(std::int64_t value) {
        return Value::append_int(value);
    }

    status_t append(const Value& value) {
        return Value::append(value);
    }

private:
    Array(doc_owner doc, const yyjson_val* value) : Value(std::move(doc), value) {}

private:
    friend class Value;
};

class Object : public Value {
public:
    Object() = default;

    static auto parse(std::string_view json) -> std::expected<Object, yyjson_read_code> {
        auto parsed = Value::parse(json);
        if(!parsed) {
            return std::unexpected(parsed.error());
        }
        auto object = parsed->as_object();
        if(!object) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }
        return std::move(*object);
    }

    static auto from_immutable_doc(yyjson_doc* raw_doc) -> std::optional<Object> {
        auto value = Value::from_immutable_doc(raw_doc);
        if(!value) {
            return std::nullopt;
        }
        return value->as_object();
    }

    [[nodiscard]] ObjectRef as_ref() const noexcept {
        if(is_mutable()) {
            return ObjectRef(mutable_value());
        }
        return ObjectRef(readable_value());
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return as_ref().size();
    }

    status_t put_int(std::string_view key, std::int64_t value) {
        return Value::put_int(key, value);
    }

    status_t put(std::string_view key, const Value& value) {
        return Value::put(key, value);
    }

private:
    Object(doc_owner doc, const yyjson_val* value) : Value(std::move(doc), value) {}

private:
    friend class Value;
};

inline auto Value::as_array() const -> std::optional<Array> {
    if(!is_array()) {
        return std::nullopt;
    }
    return Array(doc_, readable_value());
}

inline auto Value::as_object() const -> std::optional<Object> {
    if(!is_object()) {
        return std::nullopt;
    }
    return Object(doc_, readable_value());
}

}  // namespace eventide::serde::json
