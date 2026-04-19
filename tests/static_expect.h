#pragma once

#include "kota/zest/zest.h"

// Bind each operand to a `constexpr auto` first, forcing both sides to be
// constant expressions at the call site. The already-materialised values are
// then handed to the ordinary EXPECT_* macro so gtest-style diagnostics
// (value printing, file:line) stay intact.
//
// If the wrapped expression is not a constant expression, the compile error
// is localised to the `constexpr auto` binding inside the macro — not buried
// inside a static_assert in some trait instantiation chain.

#define STATIC_EXPECT_EQ(lhs, rhs)                                                                 \
    do {                                                                                           \
        constexpr auto _static_expect_lhs = (lhs);                                                 \
        constexpr auto _static_expect_rhs = (rhs);                                                 \
        EXPECT_EQ(_static_expect_lhs, _static_expect_rhs);                                         \
    } while(false)

#define STATIC_EXPECT_NE(lhs, rhs)                                                                 \
    do {                                                                                           \
        constexpr auto _static_expect_lhs = (lhs);                                                 \
        constexpr auto _static_expect_rhs = (rhs);                                                 \
        EXPECT_NE(_static_expect_lhs, _static_expect_rhs);                                         \
    } while(false)

#define STATIC_EXPECT_TRUE(expr)                                                                   \
    do {                                                                                           \
        constexpr auto _static_expect_val = (expr);                                                \
        EXPECT_TRUE(_static_expect_val);                                                           \
    } while(false)

#define STATIC_EXPECT_FALSE(expr)                                                                  \
    do {                                                                                           \
        constexpr auto _static_expect_val = (expr);                                                \
        EXPECT_FALSE(_static_expect_val);                                                          \
    } while(false)
