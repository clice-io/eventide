#pragma once

#include <cstdlib>

// Compiler/workaround feature macros shared by tests and runtime headers.

#if defined(_MSC_VER) && !defined(__clang__)
#define KOTA_COMPILER_MSVC 1
#define KOTA_COMPILER_MSVC_VERSION _MSC_VER
#else
#define KOTA_COMPILER_MSVC 0
#define KOTA_COMPILER_MSVC_VERSION 0
#endif

// Prefer [[msvc::no_unique_address]] on MSVC, [[no_unique_address]] elsewhere.
// See: https://developercommunity.visualstudio.com/t/msvc::no_unique_address-nonconforman/10504173
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(msvc::no_unique_address)
#define KOTA_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
#define KOTA_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define KOTA_NO_UNIQUE_ADDRESS
#endif
#else
#define KOTA_NO_UNIQUE_ADDRESS
#endif

// Windows ASAN (both MSVC and clang-cl) corrupts exception objects caught
// inside coroutine frames, making e.what() crash.
#if defined(_WIN32) && (defined(__SANITIZE_ADDRESS__) || defined(_CRT_USE_ADDRESS_SANITIZER))
#define KOTA_WORKAROUND_WINDOWS_ASAN_COROUTINE_EXCEPTION 1
#elif defined(_WIN32) && defined(__has_feature)
#if __has_feature(address_sanitizer)
#define KOTA_WORKAROUND_WINDOWS_ASAN_COROUTINE_EXCEPTION 1
#endif
#endif
#ifndef KOTA_WORKAROUND_WINDOWS_ASAN_COROUTINE_EXCEPTION
#define KOTA_WORKAROUND_WINDOWS_ASAN_COROUTINE_EXCEPTION 0
#endif

#if defined(KOTA_ENABLE_EXCEPTIONS)
#if KOTA_ENABLE_EXCEPTIONS && !defined(__cpp_exceptions)
#undef KOTA_ENABLE_EXCEPTIONS
#define KOTA_ENABLE_EXCEPTIONS 0
#endif
#elif defined(__cpp_exceptions)
#define KOTA_ENABLE_EXCEPTIONS 1
#else
#define KOTA_ENABLE_EXCEPTIONS 0
#endif

#if KOTA_ENABLE_EXCEPTIONS
#define KOTA_THROW(exception_expr) throw exception_expr
#define KOTA_TRY try
#define KOTA_CATCH_ALL() catch(...)
#define KOTA_RETHROW() throw
#else
#define KOTA_THROW(exception_expr)                                                                 \
    do {                                                                                           \
        static_cast<void>(sizeof(exception_expr));                                                 \
        std::abort();                                                                              \
    } while(false)
#define KOTA_TRY if(true)
#define KOTA_CATCH_ALL() else
#define KOTA_RETHROW() std::abort()
#endif
