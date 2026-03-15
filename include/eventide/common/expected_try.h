#pragma once

// ET_EXPECTED_TRY(expr)
// For std::expected<void, E> — checks for error and propagates it.
// Usage: ET_EXPECTED_TRY(d.deserialize_bool(v));
#define ET_EXPECTED_TRY(expr)                                                                      \
    do {                                                                                           \
        auto _eventide_try_result_ = (expr);                                                       \
        if(!_eventide_try_result_)                                                                 \
            return std::unexpected(std::move(_eventide_try_result_).error());                      \
    } while(0)

#define ET_CONCAT_IMPL_(a, b) a##b
#define ET_CONCAT_(a, b) ET_CONCAT_IMPL_(a, b)

// ET_EXPECTED_TRY_V(decl, expr)
// For std::expected<T, E> — checks for error, on success declares variable with the value.
// Usage: ET_EXPECTED_TRY_V(auto val, d.deserialize_seq(len));
#define ET_EXPECTED_TRY_V(decl, expr)                                                              \
    auto ET_CONCAT_(_eventide_try_tmp_, __LINE__) = (expr);                                        \
    if(!ET_CONCAT_(_eventide_try_tmp_, __LINE__))                                                  \
        return std::unexpected(std::move(ET_CONCAT_(_eventide_try_tmp_, __LINE__)).error());       \
    decl = std::move(*ET_CONCAT_(_eventide_try_tmp_, __LINE__))
