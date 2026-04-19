#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "kota/support/config.h"
#include "kota/codec/content/error.h"

namespace kota::codec::content {

class Value;
class Array;
class Object;
class Cursor;

enum class ValueKind : std::uint8_t {
    invalid = 0,
    null_value,
    boolean,
    signed_int,
    unsigned_int,
    floating,
    string,
    array,
    object,
};

namespace detail {

enum class storage_index : std::size_t {
    null_v = 0,
    bool_v = 1,
    int_v = 2,
    uint_v = 3,
    double_v = 4,
    string_v = 5,
    array_v = 6,
    object_v = 7,
};

}  // namespace detail

class Array {
public:
    using value_type = Value;
    using size_type = std::size_t;
    using iterator = typename std::vector<Value>::iterator;
    using const_iterator = typename std::vector<Value>::const_iterator;

    Array();
    Array(const Array&);
    Array(Array&&) noexcept;
    auto operator=(const Array&) -> Array&;
    auto operator=(Array&&) noexcept -> Array&;
    ~Array();

    explicit Array(std::vector<Value> items);
    Array(std::initializer_list<Value> items);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;
    void reserve(std::size_t n);

    [[nodiscard]] const Value& operator[](std::size_t index) const noexcept;
    [[nodiscard]] Value& operator[](std::size_t index) noexcept;

    [[nodiscard]] const Value& at(std::size_t index) const;
    [[nodiscard]] Value& at(std::size_t index);

    void push_back(Value value);

    template <typename... Args>
    auto emplace_back(Args&&... args) -> Value&;

    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;
    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;
    [[nodiscard]] const_iterator cbegin() const noexcept;
    [[nodiscard]] const_iterator cend() const noexcept;

    [[nodiscard]] const std::vector<Value>& items() const noexcept;
    [[nodiscard]] std::vector<Value>& items() noexcept;

    bool operator==(const Array& other) const;

private:
    std::vector<Value> items_;
};

class Object {
public:
    struct entry;

    using container_t = std::vector<entry>;
    using iterator = typename container_t::iterator;
    using const_iterator = typename container_t::const_iterator;

    Object();
    Object(const Object&);
    Object(Object&&) noexcept;
    auto operator=(const Object&) -> Object&;
    auto operator=(Object&&) noexcept -> Object&;
    ~Object();

    Object(std::initializer_list<entry> entries);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;
    void reserve(std::size_t n);

    [[nodiscard]] bool contains(std::string_view key) const;

    [[nodiscard]] const Value* find(std::string_view key) const;
    [[nodiscard]] Value* find(std::string_view key);

    [[nodiscard]] const Value& at(std::string_view key) const;
    [[nodiscard]] Value& at(std::string_view key);

    void insert(std::string key, Value value);
    void assign(std::string_view key, Value value);
    std::size_t remove(std::string_view key);

    [[nodiscard]] Value& back_value();

    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;
    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;

    [[nodiscard]] const container_t& entries() const noexcept;

    bool operator==(const Object& other) const;

private:
    void invalidate_index() noexcept;
    void ensure_index() const;

    container_t entries_;
    mutable std::optional<std::unordered_map<std::string_view, std::size_t>> index_;
};

class Value {
public:
    using storage_t = std::variant<std::monostate,
                                   bool,
                                   std::int64_t,
                                   std::uint64_t,
                                   double,
                                   std::string,
                                   Array,
                                   Object>;

    Value() noexcept;
    Value(const Value&);
    Value(Value&&) noexcept;
    auto operator=(const Value&) -> Value&;
    auto operator=(Value&&) noexcept -> Value&;
    ~Value();

    Value(std::nullptr_t) noexcept;
    Value(bool v) noexcept;
    Value(std::int64_t v) noexcept;
    Value(std::uint64_t v) noexcept;
    Value(double v) noexcept;
    Value(const char* v);
    Value(std::string_view v);
    Value(std::string v);
    Value(Array v);
    Value(Object v);

    template <std::integral T>
        requires (!std::same_as<T, bool> && !std::same_as<T, char> &&
                  !std::same_as<T, std::int64_t> && !std::same_as<T, std::uint64_t>)
    Value(T v) noexcept;

    [[nodiscard]] ValueKind kind() const noexcept;

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_bool() const noexcept;
    [[nodiscard]] bool is_int() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;

    [[nodiscard]] std::optional<bool> get_bool() const noexcept;
    [[nodiscard]] std::optional<std::int64_t> get_int() const noexcept;
    [[nodiscard]] std::optional<std::uint64_t> get_uint() const noexcept;
    [[nodiscard]] std::optional<double> get_double() const noexcept;
    [[nodiscard]] std::optional<std::string_view> get_string() const noexcept;

    [[nodiscard]] const Array* try_array() const noexcept;
    [[nodiscard]] Array* try_array() noexcept;
    [[nodiscard]] const Object* try_object() const noexcept;
    [[nodiscard]] Object* try_object() noexcept;

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] std::int64_t as_int() const;
    [[nodiscard]] std::uint64_t as_uint() const;
    [[nodiscard]] double as_double() const;
    [[nodiscard]] std::string_view as_string() const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] Array& as_array();
    [[nodiscard]] const Object& as_object() const;
    [[nodiscard]] Object& as_object();

    [[nodiscard]] Cursor as_ref() const noexcept;

    [[nodiscard]] Cursor operator[](std::string_view key) const;
    [[nodiscard]] Cursor operator[](std::size_t index) const;

    [[nodiscard]] const storage_t& storage() const noexcept;

    bool operator==(const Value& other) const;

private:
    storage_t storage_;
};

struct Object::entry {
    std::string key;
    Value value;

    bool operator==(const entry& other) const = default;
};

class Cursor {
public:
    Cursor() noexcept = default;
    explicit Cursor(const Value& value) noexcept;
    explicit Cursor(const Value* value) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool has_error() const noexcept;
    [[nodiscard]] std::string_view error() const noexcept;
    [[nodiscard]] ValueKind kind() const noexcept;

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_bool() const noexcept;
    [[nodiscard]] bool is_int() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;

    [[nodiscard]] std::optional<bool> get_bool() const noexcept;
    [[nodiscard]] std::optional<std::int64_t> get_int() const noexcept;
    [[nodiscard]] std::optional<std::uint64_t> get_uint() const noexcept;
    [[nodiscard]] std::optional<double> get_double() const noexcept;
    [[nodiscard]] std::optional<std::string_view> get_string() const noexcept;

    [[nodiscard]] const Array* try_array() const noexcept;
    [[nodiscard]] const Object* try_object() const noexcept;

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] std::int64_t as_int() const;
    [[nodiscard]] std::uint64_t as_uint() const;
    [[nodiscard]] double as_double() const;
    [[nodiscard]] std::string_view as_string() const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;

    [[nodiscard]] Cursor operator[](std::string_view key) const;
    [[nodiscard]] Cursor operator[](std::size_t index) const;

    void assert_valid() const;
    void assert_kind(ValueKind expected) const;

    [[nodiscard]] const Value* unwrap() const noexcept;

private:
    friend class Value;

    static Cursor make_error(std::string message) noexcept;

    const Value* ptr_ = nullptr;
    std::string error_;
};

// --- Array special members (defined after Value is complete) ---

inline Array::Array() = default;
inline Array::Array(const Array&) = default;
inline Array::Array(Array&&) noexcept = default;
inline auto Array::operator=(const Array&) -> Array& = default;
inline auto Array::operator=(Array&&) noexcept -> Array& = default;
inline Array::~Array() = default;

inline Array::Array(std::vector<Value> items) : items_(std::move(items)) {}

inline Array::Array(std::initializer_list<Value> items) : items_(items) {}

inline std::size_t Array::size() const noexcept {
    return items_.size();
}

inline bool Array::empty() const noexcept {
    return items_.empty();
}

inline void Array::clear() noexcept {
    items_.clear();
}

inline void Array::reserve(std::size_t n) {
    items_.reserve(n);
}

const inline Value& Array::operator[](std::size_t index) const noexcept {
    return items_[index];
}

inline Value& Array::operator[](std::size_t index) noexcept {
    return items_[index];
}

const inline Value& Array::at(std::size_t index) const {
    return items_.at(index);
}

inline Value& Array::at(std::size_t index) {
    return items_.at(index);
}

inline void Array::push_back(Value value) {
    items_.push_back(std::move(value));
}

template <typename... Args>
inline auto Array::emplace_back(Args&&... args) -> Value& {
    return items_.emplace_back(std::forward<Args>(args)...);
}

inline auto Array::begin() noexcept -> iterator {
    return items_.begin();
}

inline auto Array::end() noexcept -> iterator {
    return items_.end();
}

inline auto Array::begin() const noexcept -> const_iterator {
    return items_.begin();
}

inline auto Array::end() const noexcept -> const_iterator {
    return items_.end();
}

inline auto Array::cbegin() const noexcept -> const_iterator {
    return items_.cbegin();
}

inline auto Array::cend() const noexcept -> const_iterator {
    return items_.cend();
}

const inline std::vector<Value>& Array::items() const noexcept {
    return items_;
}

inline std::vector<Value>& Array::items() noexcept {
    return items_;
}

inline bool Array::operator==(const Array& other) const {
    return items_ == other.items_;
}

// --- Object special members and methods ---

inline Object::Object() = default;

inline Object::Object(const Object& other) : entries_(other.entries_), index_(std::nullopt) {}

inline Object::Object(Object&& other) noexcept :
    entries_(std::move(other.entries_)), index_(std::nullopt) {
    other.index_.reset();
}

inline auto Object::operator=(const Object& other) -> Object& {
    if(this != &other) {
        entries_ = other.entries_;
        index_.reset();
    }
    return *this;
}

inline auto Object::operator=(Object&& other) noexcept -> Object& {
    if(this != &other) {
        entries_ = std::move(other.entries_);
        index_.reset();
        other.index_.reset();
    }
    return *this;
}

inline Object::~Object() = default;

inline Object::Object(std::initializer_list<entry> entries) : entries_(entries) {}

inline std::size_t Object::size() const noexcept {
    return entries_.size();
}

inline bool Object::empty() const noexcept {
    return entries_.empty();
}

inline void Object::clear() noexcept {
    entries_.clear();
    invalidate_index();
}

inline void Object::reserve(std::size_t n) {
    entries_.reserve(n);
    invalidate_index();
}

inline void Object::invalidate_index() noexcept {
    index_.reset();
}

inline void Object::ensure_index() const {
    if(index_.has_value()) {
        return;
    }
    index_.emplace();
    index_->reserve(entries_.size());
    for(std::size_t i = 0; i < entries_.size(); ++i) {
        (*index_)[std::string_view(entries_[i].key)] = i;
    }
}

inline bool Object::contains(std::string_view key) const {
    return find(key) != nullptr;
}

const inline Value* Object::find(std::string_view key) const {
    ensure_index();
    auto it = index_->find(key);
    if(it == index_->end()) {
        return nullptr;
    }
    return &entries_[it->second].value;
}

inline Value* Object::find(std::string_view key) {
    return const_cast<Value*>(std::as_const(*this).find(key));
}

const inline Value& Object::at(std::string_view key) const {
    const Value* v = find(key);
    if(v == nullptr) {
        KOTA_THROW(std::out_of_range("kota::codec::content::Object::at: missing key"));
    }
    return *v;
}

inline Value& Object::at(std::string_view key) {
    Value* v = find(key);
    if(v == nullptr) {
        KOTA_THROW(std::out_of_range("kota::codec::content::Object::at: missing key"));
    }
    return *v;
}

inline void Object::insert(std::string key, Value value) {
    entries_.push_back(entry{std::move(key), std::move(value)});
    invalidate_index();
}

inline void Object::assign(std::string_view key, Value value) {
    if(Value* existing = find(key)) {
        *existing = std::move(value);
        return;
    }
    entries_.push_back(entry{std::string(key), std::move(value)});
    invalidate_index();
}

inline std::size_t Object::remove(std::string_view key) {
    auto before = entries_.size();
    std::erase_if(entries_, [&](const entry& e) { return e.key == key; });
    auto removed = before - entries_.size();
    if(removed != 0) {
        invalidate_index();
    }
    return removed;
}

inline Value& Object::back_value() {
    assert(!entries_.empty());
    return entries_.back().value;
}

inline auto Object::begin() noexcept -> iterator {
    return entries_.begin();
}

inline auto Object::end() noexcept -> iterator {
    return entries_.end();
}

inline auto Object::begin() const noexcept -> const_iterator {
    return entries_.begin();
}

inline auto Object::end() const noexcept -> const_iterator {
    return entries_.end();
}

const inline Object::container_t& Object::entries() const noexcept {
    return entries_;
}

inline bool Object::operator==(const Object& other) const {
    if(entries_.size() != other.entries_.size()) {
        return false;
    }
    std::vector<bool> matched(other.entries_.size(), false);
    for(const auto& lhs: entries_) {
        bool found = false;
        for(std::size_t j = 0; j < other.entries_.size(); ++j) {
            if(!matched[j] && other.entries_[j].key == lhs.key &&
               other.entries_[j].value == lhs.value) {
                matched[j] = true;
                found = true;
                break;
            }
        }
        if(!found) {
            return false;
        }
    }
    return true;
}

// --- Value special members and methods ---

inline Value::Value() noexcept : storage_(std::monostate{}) {}

inline Value::Value(const Value&) = default;
inline Value::Value(Value&&) noexcept = default;
inline auto Value::operator=(const Value&) -> Value& = default;
inline auto Value::operator=(Value&&) noexcept -> Value& = default;
inline Value::~Value() = default;

inline Value::Value(std::nullptr_t) noexcept : storage_(std::monostate{}) {}

inline Value::Value(bool v) noexcept : storage_(v) {}

inline Value::Value(std::int64_t v) noexcept : storage_(v) {}

inline Value::Value(std::uint64_t v) noexcept : storage_(v) {}

inline Value::Value(double v) noexcept : storage_(v) {}

inline Value::Value(const char* v) : storage_(std::string(v)) {}

inline Value::Value(std::string_view v) : storage_(std::string(v)) {}

inline Value::Value(std::string v) : storage_(std::move(v)) {}

inline Value::Value(Array v) : storage_(std::move(v)) {}

inline Value::Value(Object v) : storage_(std::move(v)) {}

template <std::integral T>
    requires (!std::same_as<T, bool> && !std::same_as<T, char> && !std::same_as<T, std::int64_t> &&
              !std::same_as<T, std::uint64_t>)
inline Value::Value(T v) noexcept {
    if constexpr(std::is_signed_v<T>) {
        storage_ = static_cast<std::int64_t>(v);
    } else {
        storage_ = static_cast<std::uint64_t>(v);
    }
}

inline ValueKind Value::kind() const noexcept {
    switch(storage_.index()) {
        case static_cast<std::size_t>(detail::storage_index::null_v): return ValueKind::null_value;
        case static_cast<std::size_t>(detail::storage_index::bool_v): return ValueKind::boolean;
        case static_cast<std::size_t>(detail::storage_index::int_v): return ValueKind::signed_int;
        case static_cast<std::size_t>(detail::storage_index::uint_v):
            return ValueKind::unsigned_int;
        case static_cast<std::size_t>(detail::storage_index::double_v): return ValueKind::floating;
        case static_cast<std::size_t>(detail::storage_index::string_v): return ValueKind::string;
        case static_cast<std::size_t>(detail::storage_index::array_v): return ValueKind::array;
        case static_cast<std::size_t>(detail::storage_index::object_v): return ValueKind::object;
        default: return ValueKind::invalid;
    }
}

inline bool Value::is_null() const noexcept {
    return std::holds_alternative<std::monostate>(storage_);
}

inline bool Value::is_bool() const noexcept {
    return std::holds_alternative<bool>(storage_);
}

inline bool Value::is_int() const noexcept {
    return std::holds_alternative<std::int64_t>(storage_) ||
           std::holds_alternative<std::uint64_t>(storage_);
}

inline bool Value::is_number() const noexcept {
    return is_int() || std::holds_alternative<double>(storage_);
}

inline bool Value::is_string() const noexcept {
    return std::holds_alternative<std::string>(storage_);
}

inline bool Value::is_array() const noexcept {
    return std::holds_alternative<Array>(storage_);
}

inline bool Value::is_object() const noexcept {
    return std::holds_alternative<Object>(storage_);
}

inline std::optional<bool> Value::get_bool() const noexcept {
    if(const auto* p = std::get_if<bool>(&storage_)) {
        return *p;
    }
    return std::nullopt;
}

inline std::optional<std::int64_t> Value::get_int() const noexcept {
    if(const auto* p = std::get_if<std::int64_t>(&storage_)) {
        return *p;
    }
    if(const auto* p = std::get_if<std::uint64_t>(&storage_)) {
        if(*p <= static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
            return static_cast<std::int64_t>(*p);
        }
    }
    return std::nullopt;
}

inline std::optional<std::uint64_t> Value::get_uint() const noexcept {
    if(const auto* p = std::get_if<std::uint64_t>(&storage_)) {
        return *p;
    }
    if(const auto* p = std::get_if<std::int64_t>(&storage_)) {
        if(*p >= 0) {
            return static_cast<std::uint64_t>(*p);
        }
    }
    return std::nullopt;
}

inline std::optional<double> Value::get_double() const noexcept {
    if(const auto* p = std::get_if<double>(&storage_)) {
        return *p;
    }
    if(const auto* p = std::get_if<std::int64_t>(&storage_)) {
        return static_cast<double>(*p);
    }
    if(const auto* p = std::get_if<std::uint64_t>(&storage_)) {
        return static_cast<double>(*p);
    }
    return std::nullopt;
}

inline std::optional<std::string_view> Value::get_string() const noexcept {
    if(const auto* p = std::get_if<std::string>(&storage_)) {
        return std::string_view(*p);
    }
    return std::nullopt;
}

const inline Array* Value::try_array() const noexcept {
    return std::get_if<Array>(&storage_);
}

inline Array* Value::try_array() noexcept {
    return std::get_if<Array>(&storage_);
}

const inline Object* Value::try_object() const noexcept {
    return std::get_if<Object>(&storage_);
}

inline Object* Value::try_object() noexcept {
    return std::get_if<Object>(&storage_);
}

inline bool Value::as_bool() const {
    return std::get<bool>(storage_);
}

inline std::int64_t Value::as_int() const {
    return std::get<std::int64_t>(storage_);
}

inline std::uint64_t Value::as_uint() const {
    return std::get<std::uint64_t>(storage_);
}

inline double Value::as_double() const {
    return std::get<double>(storage_);
}

inline std::string_view Value::as_string() const {
    const auto& s = std::get<std::string>(storage_);
    return std::string_view(s);
}

const inline Array& Value::as_array() const {
    return std::get<Array>(storage_);
}

inline Array& Value::as_array() {
    return std::get<Array>(storage_);
}

const inline Object& Value::as_object() const {
    return std::get<Object>(storage_);
}

inline Object& Value::as_object() {
    return std::get<Object>(storage_);
}

namespace detail {

inline std::string_view kind_name(ValueKind kind) noexcept {
    switch(kind) {
        case ValueKind::null_value: return "null";
        case ValueKind::boolean: return "boolean";
        case ValueKind::signed_int: return "signed_int";
        case ValueKind::unsigned_int: return "unsigned_int";
        case ValueKind::floating: return "float";
        case ValueKind::string: return "string";
        case ValueKind::array: return "array";
        case ValueKind::object: return "object";
        default: return "invalid";
    }
}

}  // namespace detail

inline Cursor Value::as_ref() const noexcept {
    return Cursor(*this);
}

inline Cursor Value::operator[](std::string_view key) const {
    if(const Object* obj = try_object()) {
        if(const Value* v = obj->find(key)) {
            return Cursor(*v);
        }
        std::string msg = "missing key \"";
        msg.append(key);
        msg.push_back('"');
        return Cursor::make_error(std::move(msg));
    }
    std::string msg = "expected object, got ";
    msg.append(detail::kind_name(kind()));
    return Cursor::make_error(std::move(msg));
}

inline Cursor Value::operator[](std::size_t index) const {
    if(const Array* arr = try_array()) {
        if(index < arr->size()) {
            return Cursor((*arr)[index]);
        }
        std::string msg = "index ";
        msg.append(std::to_string(index));
        msg.append(" out of range (size ");
        msg.append(std::to_string(arr->size()));
        msg.push_back(')');
        return Cursor::make_error(std::move(msg));
    }
    std::string msg = "expected array, got ";
    msg.append(detail::kind_name(kind()));
    return Cursor::make_error(std::move(msg));
}

const inline Value::storage_t& Value::storage() const noexcept {
    return storage_;
}

inline bool Value::operator==(const Value& other) const {
    return storage_ == other.storage_;
}

// --- Cursor ---

inline Cursor::Cursor(const Value& value) noexcept : ptr_(&value) {}

inline Cursor::Cursor(const Value* value) noexcept : ptr_(value) {}

inline Cursor Cursor::make_error(std::string message) noexcept {
    Cursor c;
    c.error_ = std::move(message);
    return c;
}

inline bool Cursor::valid() const noexcept {
    return ptr_ != nullptr;
}

inline bool Cursor::has_error() const noexcept {
    return !error_.empty();
}

inline std::string_view Cursor::error() const noexcept {
    return error_;
}

inline ValueKind Cursor::kind() const noexcept {
    return ptr_ != nullptr ? ptr_->kind() : ValueKind::invalid;
}

inline bool Cursor::is_null() const noexcept {
    return ptr_ != nullptr && ptr_->is_null();
}

inline bool Cursor::is_bool() const noexcept {
    return ptr_ != nullptr && ptr_->is_bool();
}

inline bool Cursor::is_int() const noexcept {
    return ptr_ != nullptr && ptr_->is_int();
}

inline bool Cursor::is_number() const noexcept {
    return ptr_ != nullptr && ptr_->is_number();
}

inline bool Cursor::is_string() const noexcept {
    return ptr_ != nullptr && ptr_->is_string();
}

inline bool Cursor::is_array() const noexcept {
    return ptr_ != nullptr && ptr_->is_array();
}

inline bool Cursor::is_object() const noexcept {
    return ptr_ != nullptr && ptr_->is_object();
}

inline std::optional<bool> Cursor::get_bool() const noexcept {
    return ptr_ != nullptr ? ptr_->get_bool() : std::nullopt;
}

inline std::optional<std::int64_t> Cursor::get_int() const noexcept {
    return ptr_ != nullptr ? ptr_->get_int() : std::nullopt;
}

inline std::optional<std::uint64_t> Cursor::get_uint() const noexcept {
    return ptr_ != nullptr ? ptr_->get_uint() : std::nullopt;
}

inline std::optional<double> Cursor::get_double() const noexcept {
    return ptr_ != nullptr ? ptr_->get_double() : std::nullopt;
}

inline std::optional<std::string_view> Cursor::get_string() const noexcept {
    return ptr_ != nullptr ? ptr_->get_string() : std::nullopt;
}

const inline Array* Cursor::try_array() const noexcept {
    return ptr_ != nullptr ? ptr_->try_array() : nullptr;
}

const inline Object* Cursor::try_object() const noexcept {
    return ptr_ != nullptr ? ptr_->try_object() : nullptr;
}

inline bool Cursor::as_bool() const {
    return ptr_->as_bool();
}

inline std::int64_t Cursor::as_int() const {
    return ptr_->as_int();
}

inline std::uint64_t Cursor::as_uint() const {
    return ptr_->as_uint();
}

inline double Cursor::as_double() const {
    return ptr_->as_double();
}

inline std::string_view Cursor::as_string() const {
    return ptr_->as_string();
}

const inline Array& Cursor::as_array() const {
    return ptr_->as_array();
}

const inline Object& Cursor::as_object() const {
    return ptr_->as_object();
}

inline Cursor Cursor::operator[](std::string_view key) const {
    if(ptr_ != nullptr) {
        return (*ptr_)[key];
    }
    std::string msg = error_;
    if(!msg.empty()) {
        msg.append(" -> ");
    }
    msg.append("[\"");
    msg.append(key);
    msg.append("\"]");
    return Cursor::make_error(std::move(msg));
}

inline Cursor Cursor::operator[](std::size_t index) const {
    if(ptr_ != nullptr) {
        return (*ptr_)[index];
    }
    std::string msg = error_;
    if(!msg.empty()) {
        msg.append(" -> ");
    }
    msg.append("[");
    msg.append(std::to_string(index));
    msg.append("]");
    return Cursor::make_error(std::move(msg));
}

inline void Cursor::assert_valid() const {
    assert(ptr_ != nullptr);
}

inline void Cursor::assert_kind(ValueKind expected) const {
    assert_valid();
    assert(kind() == expected);
    (void)expected;
}

const inline Value* Cursor::unwrap() const noexcept {
    return ptr_;
}

}  // namespace kota::codec::content
