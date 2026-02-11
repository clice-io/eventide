#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "serde/traits.h"
#include "serde/yyjson/dom.h"

namespace eventide::serde::detail {

template <>
struct error_traits<yyjson_read_code> {
    constexpr static yyjson_read_code invalid_argument() {
        return YYJSON_READ_ERROR_INVALID_PARAMETER;
    }

    constexpr static yyjson_read_code no_data_available() {
        return YYJSON_READ_ERROR_EMPTY_CONTENT;
    }
};

}  // namespace eventide::serde::detail

namespace eventide::serde::json::yy {

class Deserializer {
public:
    using object_field = Dom::object_member;
    using value_type = Dom::value_type;
    using object_type = std::vector<object_field>;
    using array_type = std::vector<value_type>;
    using error_type = Dom::read_error_type;

    template <class T>
    using result_t = std::expected<T, error_type>;

    Deserializer() = default;

    explicit Deserializer(std::string_view json) {
        parse(json);
    }

    explicit Deserializer(Dom dom) {
        parse(std::move(dom));
    }

    ~Deserializer() = default;

    Deserializer(const Deserializer&) = delete;
    Deserializer& operator=(const Deserializer&) = delete;
    Deserializer(Deserializer&&) = delete;
    Deserializer& operator=(Deserializer&&) = delete;

    result_t<void> parse(std::string_view json) {
        clear();

        auto parsed = Dom::parse(json);
        if(!parsed) {
            last_error = parsed.error();
            return std::unexpected(last_error);
        }

        dom = std::move(*parsed);
        last_error = YYJSON_READ_SUCCESS;
        return {};
    }

    result_t<void> parse(const std::string& json) {
        return parse(std::string_view(json));
    }

    result_t<void> parse(Dom&& input_dom) {
        clear();

        if(!input_dom.valid()) {
            last_error = YYJSON_READ_ERROR_INVALID_PARAMETER;
            return std::unexpected(last_error);
        }

        this->dom = std::move(input_dom);
        last_error = YYJSON_READ_SUCCESS;
        return {};
    }

    result_t<void> parse(const Dom& input_dom) {
        clear();

        auto copied = input_dom.copy();
        if(!copied) {
            last_error = copied.error();
            return std::unexpected(last_error);
        }

        this->dom = std::move(*copied);
        last_error = YYJSON_READ_SUCCESS;
        return {};
    }

    result_t<value_type> root() const {
        if(!dom.valid()) {
            const auto error =
                last_error == YYJSON_READ_SUCCESS ? YYJSON_READ_ERROR_EMPTY_CONTENT : last_error;
            return std::unexpected(error);
        }

        auto* root_node = dom.root();
        if(root_node == nullptr) {
            return std::unexpected(YYJSON_READ_ERROR_EMPTY_CONTENT);
        }
        return root_node;
    }

    void clear() {
        dom.reset();
    }

private:
    template <class T, class D>
    friend eventide::serde::deserialize_result_t<T, D>
        eventide::serde::deserialize(D& deserializer, value_type_t<D> value);

    result_t<object_type> as_object(value_type value) const {
        auto fields = dom.object_members(value);
        if(!fields) {
            return std::unexpected(fields.error());
        }
        return std::move(*fields);
    }

    result_t<array_type> as_array(value_type value) const {
        auto items = dom.array_elements(value);
        if(!items) {
            return std::unexpected(items.error());
        }
        return std::move(*items);
    }

    result_t<std::string_view> as_string(value_type value) const {
        return dom.as_string(value);
    }

    result_t<std::int64_t> as_i64(value_type value) const {
        return dom.as_i64(value);
    }

    result_t<std::uint64_t> as_u64(value_type value) const {
        return dom.as_u64(value);
    }

    result_t<double> as_f64(value_type value) const {
        return dom.as_f64(value);
    }

    result_t<bool> as_bool(value_type value) const {
        return dom.as_bool(value);
    }

    bool is_null(value_type value) const {
        return dom.is_null(value);
    }

    Dom dom{};
    error_type last_error = YYJSON_READ_SUCCESS;
};

}  // namespace eventide::serde::json::yy
