#pragma once

// Compiler/workaround feature macros shared by tests and runtime headers.

#if defined(_MSC_VER) && !defined(__clang__)
#define ETD_COMPILER_MSVC 1
#define ETD_COMPILER_MSVC_VERSION _MSC_VER
#else
#define ETD_COMPILER_MSVC 0
#define ETD_COMPILER_MSVC_VERSION 0
#endif

// Visual Studio issue:
// https://developercommunity.visualstudio.com/t/Unable-to-destroy-C20-coroutine-in-fin/10657377
//
// Reported fixed in VS 2026 toolset v145, still reproducible in v143.
// We treat _MSC_VER < 1950 as affected.
#if ETD_COMPILER_MSVC && (ETD_COMPILER_MSVC_VERSION < 1950) &&                                     \
    (defined(_CRT_USE_ADDRESS_SANITIZER) || defined(__SANITIZE_ADDRESS__))
#define ETD_WORKAROUND_MSVC_COROUTINE_ASAN_UAF 1
#else
#define ETD_WORKAROUND_MSVC_COROUTINE_ASAN_UAF 0
#endif

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(msvc::no_unique_address)
#define ETD_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
#define ETD_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define ETD_NO_UNIQUE_ADDRESS
#endif
#else
#define ETD_NO_UNIQUE_ADDRESS
#endif

#if defined(ETD_ENABLE_EXCEPTIONS)
#if ETD_ENABLE_EXCEPTIONS && !defined(__cpp_exceptions)
#undef ETD_ENABLE_EXCEPTIONS
#define ETD_ENABLE_EXCEPTIONS 0
#endif
#elif defined(__cpp_exceptions)
#define ETD_ENABLE_EXCEPTIONS 1
#else
#define ETD_ENABLE_EXCEPTIONS 0
#endif
