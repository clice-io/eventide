#pragma once

#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace eventide::serde {

template <typename A, typename T, typename E>
concept result_as = std::same_as<A, std::expected<T, E>>;

namespace detail {

template <typename E>
struct concept_bool_visitor {
    using value_type = bool;
    std::string_view expecting() const;
    std::expected<bool, E> visit_bool(bool);
};

template <typename E>
struct concept_i64_visitor {
    using value_type = std::int64_t;
    std::string_view expecting() const;
    std::expected<std::int64_t, E> visit_i64(std::int64_t);
};

template <typename E>
struct concept_u64_visitor {
    using value_type = std::uint64_t;
    std::string_view expecting() const;
    std::expected<std::uint64_t, E> visit_u64(std::uint64_t);
};

template <typename E>
struct concept_f64_visitor {
    using value_type = double;
    std::string_view expecting() const;
    std::expected<double, E> visit_f64(double);
};

template <typename E>
struct concept_char_visitor {
    using value_type = char;
    std::string_view expecting() const;
    std::expected<char, E> visit_char(char);
};

template <typename E>
struct concept_string_visitor {
    using value_type = std::string;
    std::string_view expecting() const;
    std::expected<std::string, E> visit_string(std::string);
    std::expected<std::string, E> visit_str(std::string_view);
};

template <typename E>
struct concept_option_visitor {
    using value_type = std::int32_t;
    std::string_view expecting() const;
    std::expected<std::int32_t, E> visit_none();
    template <typename D>
    std::expected<std::int32_t, E> visit_some(D&);
};

template <typename E>
struct concept_seq_visitor {
    using value_type = std::int32_t;
    std::string_view expecting() const;
    template <typename SeqAccess>
    std::expected<std::int32_t, E> visit_seq(SeqAccess&);
};

template <typename E>
struct concept_map_visitor {
    using value_type = std::int32_t;
    std::string_view expecting() const;
    template <typename MapAccess>
    std::expected<std::int32_t, E> visit_map(MapAccess&);
};

template <typename E>
struct concept_ignored_any_visitor {
    using value_type = void;
    std::string_view expecting() const;
    std::expected<void, E> visit_ignored_any();
};

}  // namespace detail

template <typename S,
          typename T = typename S::value_type,
          typename E = typename S::error_type,
          typename SerializeSeq = typename S::SerializeSeq,
          typename SerializeTuple = typename S::SerializeTuple,
          typename SerializeMap = typename S::SerializeMap,
          typename SerializeStruct = typename S::SerializeStruct>
concept serializer_like = requires(S& s,
                                   bool b,
                                   char c,
                                   std::int64_t i,
                                   std::uint64_t u,
                                   double f,
                                   std::string_view text,
                                   std::optional<std::size_t> len,
                                   std::size_t tuple_len,
                                   const int& key,
                                   const int& value) {
    { s.serialize_none() } -> result_as<T, E>;
    requires (
        requires {
            { s.serialize_some(i) } -> result_as<T, E>;
        } ||
        requires {
            { s.serialize_some(i) } -> std::same_as<void>;
        });

    { s.serialize_bool(b) } -> result_as<T, E>;
    { s.serialize_int(i) } -> result_as<T, E>;
    { s.serialize_uint(u) } -> result_as<T, E>;
    { s.serialize_float(f) } -> result_as<T, E>;
    { s.serialize_char(c) } -> result_as<T, E>;
    { s.serialize_str(text) } -> result_as<T, E>;
    { s.serialize_bytes(text) } -> result_as<T, E>;

    { s.serialize_seq(len) } -> result_as<SerializeSeq, E>;
    requires requires(SerializeSeq& s) {
        { s.serialize_element(value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };

    { s.serialize_tuple(tuple_len) } -> result_as<SerializeTuple, E>;
    requires requires(SerializeTuple& s) {
        { s.serialize_element(value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };

    { s.serialize_map(len) } -> result_as<SerializeMap, E>;
    requires requires(SerializeMap& s) {
        { s.serialize_entry(key, value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };

    { s.serialize_struct(text, tuple_len) } -> result_as<SerializeStruct, E>;
    requires requires(SerializeStruct& s) {
        { s.serialize_field(text, value) } -> result_as<void, E>;
        { s.end() } -> result_as<T, E>;
    };
};

}  // namespace eventide::serde
