#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if __has_include(<yyjson.h>)
#include <yyjson.h>
#elif __has_include(<rfl/thirdparty/yyjson.h>)
#include <rfl/thirdparty/yyjson.h>
#else
#error "yyjson.h not found. Enable EVENTIDE_ENABLE_LANGUAGE or add yyjson include paths."
#endif

namespace eventide::serde::json::yy {

class MutableDom;

namespace detail {

template <class ValueType>
struct object_member {
    std::string_view key;
    ValueType value = nullptr;
};

template <class ValueType>
struct value_ops;

template <>
struct value_ops<yyjson_val*> {
    static bool is_null(yyjson_val* value) {
        return yyjson_is_null(value);
    }

    static bool is_bool(yyjson_val* value) {
        return yyjson_is_bool(value);
    }

    static bool is_sint(yyjson_val* value) {
        return yyjson_is_sint(value);
    }

    static bool is_uint(yyjson_val* value) {
        return yyjson_is_uint(value);
    }

    static bool is_num(yyjson_val* value) {
        return yyjson_is_num(value);
    }

    static bool is_str(yyjson_val* value) {
        return yyjson_is_str(value);
    }

    static bool is_arr(yyjson_val* value) {
        return yyjson_is_arr(value);
    }

    static bool is_obj(yyjson_val* value) {
        return yyjson_is_obj(value);
    }

    static std::size_t arr_size(yyjson_val* value) {
        return yyjson_arr_size(value);
    }

    static std::size_t obj_size(yyjson_val* value) {
        return yyjson_obj_size(value);
    }

    static yyjson_val* arr_get(yyjson_val* value, std::size_t index) {
        return yyjson_arr_get(value, index);
    }

    static yyjson_val* obj_getn(yyjson_val* value, std::string_view key) {
        return yyjson_obj_getn(value, key.data(), key.size());
    }

    const static char* get_str(yyjson_val* value) {
        return yyjson_get_str(value);
    }

    static std::size_t get_len(yyjson_val* value) {
        return yyjson_get_len(value);
    }

    static bool get_bool(yyjson_val* value) {
        return yyjson_get_bool(value);
    }

    static std::int64_t get_sint(yyjson_val* value) {
        return yyjson_get_sint(value);
    }

    static std::uint64_t get_uint(yyjson_val* value) {
        return yyjson_get_uint(value);
    }

    static double get_num(yyjson_val* value) {
        return yyjson_get_num(value);
    }

    template <class Fn>
    static void for_each_object(yyjson_val* value, Fn&& fn) {
        std::size_t idx = 0;
        std::size_t max = 0;
        yyjson_val* key = nullptr;
        yyjson_val* item = nullptr;
        yyjson_obj_foreach(value, idx, max, key, item) {
            fn(key, item);
        }
    }

    template <class Fn>
    static void for_each_array(yyjson_val* value, Fn&& fn) {
        std::size_t idx = 0;
        std::size_t max = 0;
        yyjson_val* item = nullptr;
        yyjson_arr_foreach(value, idx, max, item) {
            fn(item);
        }
    }
};

template <>
struct value_ops<yyjson_mut_val*> {
    static bool is_null(yyjson_mut_val* value) {
        return yyjson_mut_is_null(value);
    }

    static bool is_bool(yyjson_mut_val* value) {
        return yyjson_mut_is_bool(value);
    }

    static bool is_sint(yyjson_mut_val* value) {
        return yyjson_mut_is_sint(value);
    }

    static bool is_uint(yyjson_mut_val* value) {
        return yyjson_mut_is_uint(value);
    }

    static bool is_num(yyjson_mut_val* value) {
        return yyjson_mut_is_num(value);
    }

    static bool is_str(yyjson_mut_val* value) {
        return yyjson_mut_is_str(value);
    }

    static bool is_arr(yyjson_mut_val* value) {
        return yyjson_mut_is_arr(value);
    }

    static bool is_obj(yyjson_mut_val* value) {
        return yyjson_mut_is_obj(value);
    }

    static std::size_t arr_size(yyjson_mut_val* value) {
        return yyjson_mut_arr_size(value);
    }

    static std::size_t obj_size(yyjson_mut_val* value) {
        return yyjson_mut_obj_size(value);
    }

    static yyjson_mut_val* arr_get(yyjson_mut_val* value, std::size_t index) {
        return yyjson_mut_arr_get(value, index);
    }

    static yyjson_mut_val* obj_getn(yyjson_mut_val* value, std::string_view key) {
        return yyjson_mut_obj_getn(value, key.data(), key.size());
    }

    const static char* get_str(yyjson_mut_val* value) {
        return yyjson_mut_get_str(value);
    }

    static std::size_t get_len(yyjson_mut_val* value) {
        return yyjson_mut_get_len(value);
    }

    static bool get_bool(yyjson_mut_val* value) {
        return yyjson_mut_get_bool(value);
    }

    static std::int64_t get_sint(yyjson_mut_val* value) {
        return yyjson_mut_get_sint(value);
    }

    static std::uint64_t get_uint(yyjson_mut_val* value) {
        return yyjson_mut_get_uint(value);
    }

    static double get_num(yyjson_mut_val* value) {
        return yyjson_mut_get_num(value);
    }

    template <class Fn>
    static void for_each_object(yyjson_mut_val* value, Fn&& fn) {
        std::size_t idx = 0;
        std::size_t max = 0;
        yyjson_mut_val* key = nullptr;
        yyjson_mut_val* item = nullptr;
        yyjson_mut_obj_foreach(value, idx, max, key, item) {
            fn(key, item);
        }
    }

    template <class Fn>
    static void for_each_array(yyjson_mut_val* value, Fn&& fn) {
        std::size_t idx = 0;
        std::size_t max = 0;
        yyjson_mut_val* item = nullptr;
        yyjson_mut_arr_foreach(value, idx, max, item) {
            fn(item);
        }
    }
};

template <class Derived, class ValueType, class ObjectMember>
class read_dom_base {
public:
    using value_type = ValueType;
    using object_member = ObjectMember;
    using read_error_type = yyjson_read_code;

    bool is_null(value_type node = nullptr) const {
        return value_ops<value_type>::is_null(resolve(node));
    }

    bool is_bool(value_type node = nullptr) const {
        return value_ops<value_type>::is_bool(resolve(node));
    }

    bool is_i64(value_type node = nullptr) const {
        return value_ops<value_type>::is_sint(resolve(node));
    }

    bool is_u64(value_type node = nullptr) const {
        return value_ops<value_type>::is_uint(resolve(node));
    }

    bool is_f64(value_type node = nullptr) const {
        return value_ops<value_type>::is_num(resolve(node));
    }

    bool is_string(value_type node = nullptr) const {
        return value_ops<value_type>::is_str(resolve(node));
    }

    bool is_array(value_type node = nullptr) const {
        return value_ops<value_type>::is_arr(resolve(node));
    }

    bool is_object(value_type node = nullptr) const {
        return value_ops<value_type>::is_obj(resolve(node));
    }

    std::size_t array_size(value_type node = nullptr) const {
        return value_ops<value_type>::arr_size(resolve(node));
    }

    std::size_t object_size(value_type node = nullptr) const {
        return value_ops<value_type>::obj_size(resolve(node));
    }

    value_type at(std::size_t index, value_type array_node = nullptr) const {
        return value_ops<value_type>::arr_get(resolve(array_node), index);
    }

    value_type get(std::string_view key, value_type object_node = nullptr) const {
        return value_ops<value_type>::obj_getn(resolve(object_node), key);
    }

    std::expected<bool, read_error_type> as_bool(value_type node = nullptr) const {
        auto* value = resolve(node);
        if(!value_ops<value_type>::is_bool(value)) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }
        return value_ops<value_type>::get_bool(value);
    }

    std::expected<std::int64_t, read_error_type> as_i64(value_type node = nullptr) const {
        auto* value = resolve(node);
        if(value_ops<value_type>::is_sint(value)) {
            return value_ops<value_type>::get_sint(value);
        }
        if(value_ops<value_type>::is_uint(value)) {
            const auto number = value_ops<value_type>::get_uint(value);
            if(number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
            }
            return static_cast<std::int64_t>(number);
        }
        return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
    }

    std::expected<std::uint64_t, read_error_type> as_u64(value_type node = nullptr) const {
        auto* value = resolve(node);
        if(value_ops<value_type>::is_uint(value)) {
            return value_ops<value_type>::get_uint(value);
        }
        if(value_ops<value_type>::is_sint(value)) {
            const auto number = value_ops<value_type>::get_sint(value);
            if(number < 0) {
                return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
            }
            return static_cast<std::uint64_t>(number);
        }
        return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
    }

    std::expected<double, read_error_type> as_f64(value_type node = nullptr) const {
        auto* value = resolve(node);
        if(!value_ops<value_type>::is_num(value)) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }
        return value_ops<value_type>::get_num(value);
    }

    std::expected<std::string_view, read_error_type> as_string(value_type node = nullptr) const {
        auto* value = resolve(node);
        if(!value_ops<value_type>::is_str(value)) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }

        const char* text = value_ops<value_type>::get_str(value);
        std::size_t len = value_ops<value_type>::get_len(value);
        if(text == nullptr && len != 0) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }
        return std::string_view(text == nullptr ? "" : text, len);
    }

    std::expected<std::vector<object_member>, read_error_type>
        object_members(value_type node = nullptr) const {
        auto* object = resolve(node);
        if(!value_ops<value_type>::is_obj(object)) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }

        std::vector<object_member> fields{};
        fields.reserve(value_ops<value_type>::obj_size(object));

        bool valid = true;
        value_ops<value_type>::for_each_object(object, [&](value_type key, value_type item) {
            const char* key_text = value_ops<value_type>::get_str(key);
            std::size_t key_len = value_ops<value_type>::get_len(key);
            if(key_text == nullptr && key_len != 0) {
                valid = false;
                return;
            }
            fields.push_back(object_member{
                .key = std::string_view(key_text == nullptr ? "" : key_text, key_len),
                .value = item,
            });
        });

        if(!valid) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }
        return fields;
    }

    std::expected<std::vector<value_type>, read_error_type>
        array_elements(value_type node = nullptr) const {
        auto* array = resolve(node);
        if(!value_ops<value_type>::is_arr(array)) {
            return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
        }

        std::vector<value_type> items{};
        items.reserve(value_ops<value_type>::arr_size(array));

        value_ops<value_type>::for_each_array(array,
                                              [&](value_type item) { items.push_back(item); });
        return items;
    }

protected:
    value_type resolve(value_type node) const {
        return node == nullptr ? derived().root() : node;
    }

private:
    const Derived& derived() const {
        return static_cast<const Derived&>(*this);
    }
};

}  // namespace detail

class Dom : public detail::read_dom_base<Dom, yyjson_val*, detail::object_member<yyjson_val*>> {
public:
    using base_type = detail::read_dom_base<Dom, yyjson_val*, detail::object_member<yyjson_val*>>;
    using read_error_type = yyjson_read_code;
    using write_error_type = yyjson_write_code;
    using value_type = yyjson_val*;
    using object_member = detail::object_member<value_type>;
    using base_type::get;

    Dom() = default;

    explicit Dom(yyjson_doc* document) : document(document) {}

    ~Dom() {
        reset();
    }

    Dom(const Dom&) = delete;
    Dom& operator=(const Dom&) = delete;

    Dom(Dom&& other) noexcept : document(std::exchange(other.document, nullptr)) {}

    Dom& operator=(Dom&& other) noexcept {
        if(this == &other) {
            return *this;
        }
        reset();
        document = std::exchange(other.document, nullptr);
        return *this;
    }

    bool valid() const {
        return document != nullptr;
    }

    value_type root() const {
        return document == nullptr ? nullptr : yyjson_doc_get_root(document);
    }

    yyjson_doc* get() const {
        return document;
    }

    void reset(yyjson_doc* new_document = nullptr) {
        if(document != nullptr) {
            yyjson_doc_free(document);
        }
        document = new_document;
    }

    std::expected<std::string, write_error_type> str() const {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }

        yyjson_write_err err{};
        std::size_t len = 0;
        char* json = yyjson_write_opts(document, YYJSON_WRITE_NOFLAG, nullptr, &len, &err);
        if(json == nullptr) {
            const auto code =
                err.code == YYJSON_WRITE_SUCCESS ? YYJSON_WRITE_ERROR_MEMORY_ALLOCATION : err.code;
            return std::unexpected(code);
        }

        std::string out(json, len);
        std::free(json);
        return out;
    }

    std::expected<Dom, read_error_type> copy() const;

    std::expected<MutableDom, read_error_type> thaw() const;

private:
    static read_error_type map_write_to_read(write_error_type error) {
        switch(error) {
            case YYJSON_WRITE_ERROR_MEMORY_ALLOCATION: return YYJSON_READ_ERROR_MEMORY_ALLOCATION;
            default: return YYJSON_READ_ERROR_INVALID_PARAMETER;
        }
    }

    yyjson_doc* document = nullptr;

    friend class MutableDom;
};

class MutableDom :
    public detail::
        read_dom_base<MutableDom, yyjson_mut_val*, detail::object_member<yyjson_mut_val*>> {
public:
    using base_type =
        detail::read_dom_base<MutableDom, yyjson_mut_val*, detail::object_member<yyjson_mut_val*>>;
    using read_error_type = yyjson_read_code;
    using write_error_type = yyjson_write_code;
    using value_type = yyjson_mut_val*;
    using object_member = detail::object_member<value_type>;
    using base_type::get;

    template <class T>
    using result_t = std::expected<T, write_error_type>;

    MutableDom() {
        clear();
    }

    explicit MutableDom(yyjson_mut_doc* document) : document(document) {}

    ~MutableDom() {
        reset();
    }

    MutableDom(const MutableDom&) = delete;
    MutableDom& operator=(const MutableDom&) = delete;

    MutableDom(MutableDom&& other) noexcept : document(std::exchange(other.document, nullptr)) {}

    MutableDom& operator=(MutableDom&& other) noexcept {
        if(this == &other) {
            return *this;
        }
        reset();
        document = std::exchange(other.document, nullptr);
        return *this;
    }

    static result_t<MutableDom> object() {
        MutableDom dom{};
        auto root = dom.create_object();
        if(!root) {
            return std::unexpected(root.error());
        }
        auto set = dom.set_root_value(*root);
        if(!set) {
            return std::unexpected(set.error());
        }
        return std::move(dom);
    }

    static result_t<MutableDom> array() {
        MutableDom dom{};
        auto root = dom.create_array();
        if(!root) {
            return std::unexpected(root.error());
        }
        auto set = dom.set_root_value(*root);
        if(!set) {
            return std::unexpected(set.error());
        }
        return std::move(dom);
    }

    void clear() {
        reset(yyjson_mut_doc_new(nullptr));
    }

    bool valid() const {
        return document != nullptr;
    }

    value_type root() const {
        return document == nullptr ? nullptr : yyjson_mut_doc_get_root(document);
    }

    void reset(yyjson_mut_doc* new_document = nullptr) {
        if(document != nullptr) {
            yyjson_mut_doc_free(document);
        }
        document = new_document;
    }

    std::expected<std::string, write_error_type> str() const {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }

        yyjson_write_err err{};
        std::size_t len = 0;
        char* json = yyjson_mut_write_opts(document, YYJSON_WRITE_NOFLAG, nullptr, &len, &err);
        if(json == nullptr) {
            const auto code =
                err.code == YYJSON_WRITE_SUCCESS ? YYJSON_WRITE_ERROR_MEMORY_ALLOCATION : err.code;
            return std::unexpected(code);
        }

        std::string out(json, len);
        std::free(json);
        return out;
    }

    std::expected<Dom, write_error_type> freeze() const {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }

        auto* immutable_document = yyjson_mut_doc_imut_copy(document, nullptr);
        if(immutable_document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return Dom(immutable_document);
    }

    result_t<value_type> create_null() {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* value = yyjson_mut_null(document);
        if(value == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return value;
    }

    result_t<value_type> create_bool(bool value) {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* node = yyjson_mut_bool(document, value);
        if(node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return node;
    }

    result_t<value_type> create_i64(std::int64_t value) {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* node = yyjson_mut_sint(document, value);
        if(node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return node;
    }

    result_t<value_type> create_u64(std::uint64_t value) {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* node = yyjson_mut_uint(document, value);
        if(node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return node;
    }

    result_t<value_type> create_f64(double value) {
        if(!std::isfinite(value)) {
            return create_null();
        }
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* node = yyjson_mut_real(document, value);
        if(node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return node;
    }

    result_t<value_type> create_string(std::string_view value) {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* node = yyjson_mut_strncpy(document, value.data(), value.size());
        if(node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return node;
    }

    result_t<value_type> create_object() {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* node = yyjson_mut_obj(document);
        if(node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return node;
    }

    result_t<value_type> create_array() {
        if(document == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        auto* node = yyjson_mut_arr(document);
        if(node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return node;
    }

    result_t<void> set_root_value(value_type value) {
        if(document == nullptr || value == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        yyjson_mut_doc_set_root(document, value);
        return {};
    }

    result_t<void> append_array_value(value_type array, value_type value) {
        if(!yyjson_mut_is_arr(array) || value == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        if(!yyjson_mut_arr_add_val(array, value)) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return {};
    }

    result_t<void> add_object_value(value_type object, std::string_view key, value_type value) {
        if(document == nullptr || !yyjson_mut_is_obj(object) || value == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }

        auto* key_node = yyjson_mut_strncpy(document, key.data(), key.size());
        if(key_node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }

        if(!yyjson_mut_obj_add(object, key_node, value)) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return {};
    }

    result_t<void> put_object_value(value_type object, std::string_view key, value_type value) {
        if(document == nullptr || !yyjson_mut_is_obj(object) || value == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }

        auto* key_node = yyjson_mut_strncpy(document, key.data(), key.size());
        if(key_node == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }

        if(!yyjson_mut_obj_put(object, key_node, value)) {
            return std::unexpected(YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
        }
        return {};
    }

    result_t<void> set_null(std::string_view key) {
        auto object = ensure_root_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        auto value = create_null();
        if(!value) {
            return std::unexpected(value.error());
        }
        return put_object_value(*object, key, *value);
    }

    result_t<void> set_bool(std::string_view key, bool value) {
        auto object = ensure_root_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        auto node = create_bool(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return put_object_value(*object, key, *node);
    }

    result_t<void> set_i64(std::string_view key, std::int64_t value) {
        auto object = ensure_root_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        auto node = create_i64(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return put_object_value(*object, key, *node);
    }

    result_t<void> set_u64(std::string_view key, std::uint64_t value) {
        auto object = ensure_root_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        auto node = create_u64(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return put_object_value(*object, key, *node);
    }

    result_t<void> set_f64(std::string_view key, double value) {
        auto object = ensure_root_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        auto node = create_f64(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return put_object_value(*object, key, *node);
    }

    result_t<void> set_string(std::string_view key, std::string_view value) {
        auto object = ensure_root_object();
        if(!object) {
            return std::unexpected(object.error());
        }

        auto node = create_string(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return put_object_value(*object, key, *node);
    }

    result_t<void> erase(std::string_view key) {
        auto* object = root();
        if(!yyjson_mut_is_obj(object)) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        yyjson_mut_obj_remove_strn(object, key.data(), key.size());
        return {};
    }

    result_t<void> push_null() {
        auto array = ensure_root_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        auto value = create_null();
        if(!value) {
            return std::unexpected(value.error());
        }
        return append_array_value(*array, *value);
    }

    result_t<void> push_bool(bool value) {
        auto array = ensure_root_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        auto node = create_bool(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return append_array_value(*array, *node);
    }

    result_t<void> push_i64(std::int64_t value) {
        auto array = ensure_root_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        auto node = create_i64(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return append_array_value(*array, *node);
    }

    result_t<void> push_u64(std::uint64_t value) {
        auto array = ensure_root_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        auto node = create_u64(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return append_array_value(*array, *node);
    }

    result_t<void> push_f64(double value) {
        auto array = ensure_root_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        auto node = create_f64(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return append_array_value(*array, *node);
    }

    result_t<void> push_string(std::string_view value) {
        auto array = ensure_root_array();
        if(!array) {
            return std::unexpected(array.error());
        }

        auto node = create_string(value);
        if(!node) {
            return std::unexpected(node.error());
        }
        return append_array_value(*array, *node);
    }

    result_t<void> remove_at(std::size_t index) {
        auto* array = root();
        if(!yyjson_mut_is_arr(array)) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        if(yyjson_mut_arr_remove(array, index) == nullptr) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        return {};
    }

private:
    result_t<value_type> ensure_root_object() {
        auto* object = root();
        if(object == nullptr) {
            auto created = create_object();
            if(!created) {
                return std::unexpected(created.error());
            }
            auto set = set_root_value(*created);
            if(!set) {
                return std::unexpected(set.error());
            }
            object = *created;
        }

        if(!yyjson_mut_is_obj(object)) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        return object;
    }

    result_t<value_type> ensure_root_array() {
        auto* array = root();
        if(array == nullptr) {
            auto created = create_array();
            if(!created) {
                return std::unexpected(created.error());
            }
            auto set = set_root_value(*created);
            if(!set) {
                return std::unexpected(set.error());
            }
            array = *created;
        }

        if(!yyjson_mut_is_arr(array)) {
            return std::unexpected(YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        }
        return array;
    }

    yyjson_mut_doc* document = nullptr;
};

inline std::expected<MutableDom, Dom::read_error_type> Dom::thaw() const {
    if(document == nullptr) {
        return std::unexpected(YYJSON_READ_ERROR_INVALID_PARAMETER);
    }

    auto* mutable_document = yyjson_doc_mut_copy(document, nullptr);
    if(mutable_document == nullptr) {
        return std::unexpected(YYJSON_READ_ERROR_MEMORY_ALLOCATION);
    }
    return MutableDom(mutable_document);
}

inline std::expected<Dom, Dom::read_error_type> Dom::copy() const {
    auto mutable_document = thaw();
    if(!mutable_document) {
        return std::unexpected(mutable_document.error());
    }

    auto immutable_document = mutable_document->freeze();
    if(!immutable_document) {
        return std::unexpected(map_write_to_read(immutable_document.error()));
    }
    return std::move(*immutable_document);
}

inline std::expected<Dom, Dom::read_error_type> parse(std::string_view json) {
    yyjson_read_err err{};
    auto* parsed = yyjson_read_opts(const_cast<char*>(json.data()),
                                    json.size(),
                                    YYJSON_READ_NOFLAG,
                                    nullptr,
                                    &err);
    if(parsed == nullptr) {
        const auto code =
            err.code == YYJSON_READ_SUCCESS ? YYJSON_READ_ERROR_INVALID_PARAMETER : err.code;
        return std::unexpected(code);
    }
    return Dom(parsed);
}

}  // namespace eventide::serde::json::yy
