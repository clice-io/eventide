cmake_minimum_required(VERSION 3.16)

project(eventide LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(EVENTIDE_ENABLE_ASYNC "Enable async eventide target and libuv dependency" ON)
option(EVENTIDE_ENABLE_TEST "Build unit tests" OFF)
option(EVENTIDE_ENABLE_ZEST "Build zest test framework target" OFF)
option(EVENTIDE_SERDE_ENABLE_SIMDJSON "Enable simdjson dependency for eventide::serde" OFF)
option(EVENTIDE_SERDE_ENABLE_FLATBUFFERS "Enable flatbuffers dependency for eventide::serde" OFF)
option(EVENTIDE_BUILD_ALL_TESTS "Enable all optional test suites for CI" OFF)

if(EVENTIDE_BUILD_ALL_TESTS)
    if(NOT EVENTIDE_ENABLE_TEST)
        message(STATUS "EVENTIDE_BUILD_ALL_TESTS=ON: enabling EVENTIDE_ENABLE_TEST")
        set(EVENTIDE_ENABLE_TEST ON CACHE BOOL "Build unit tests" FORCE)
    endif()

    if(NOT EVENTIDE_ENABLE_ASYNC)
        message(STATUS "EVENTIDE_BUILD_ALL_TESTS=ON: enabling EVENTIDE_ENABLE_ASYNC")
        set(EVENTIDE_ENABLE_ASYNC ON CACHE BOOL "Enable async eventide target and libuv dependency" FORCE)
    endif()

    if(NOT EVENTIDE_SERDE_ENABLE_SIMDJSON)
        message(STATUS "EVENTIDE_BUILD_ALL_TESTS=ON: enabling EVENTIDE_SERDE_ENABLE_SIMDJSON")
        set(EVENTIDE_SERDE_ENABLE_SIMDJSON ON CACHE BOOL "Enable simdjson dependency for eventide::serde" FORCE)
    endif()

    if(NOT EVENTIDE_SERDE_ENABLE_FLATBUFFERS)
        message(STATUS "EVENTIDE_BUILD_ALL_TESTS=ON: enabling EVENTIDE_SERDE_ENABLE_FLATBUFFERS")
        set(EVENTIDE_SERDE_ENABLE_FLATBUFFERS ON CACHE BOOL "Enable flatbuffers dependency for eventide::serde" FORCE)
    endif()
endif()

if(EVENTIDE_ENABLE_TEST)
    if(NOT EVENTIDE_ENABLE_ZEST)
        message(STATUS "EVENTIDE_ENABLE_TEST=ON: enabling EVENTIDE_ENABLE_ZEST")
        set(EVENTIDE_ENABLE_ZEST ON CACHE BOOL "Build zest test framework target" FORCE)
    endif()

    if(MSVC)
        add_compile_options("/Zc:preprocessor")
    else()
        add_compile_options("-fsanitize=address" "-g")
        add_link_options("-fsanitize=address" "-g")
    endif()
endif()

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

add_subdirectory(src/reflection)
add_subdirectory(src/serde)

if(EVENTIDE_ENABLE_ASYNC)
    add_subdirectory(src/eventide)
endif()

if(EVENTIDE_ENABLE_ASYNC AND EVENTIDE_SERDE_ENABLE_SIMDJSON)
    add_subdirectory(src/language)
endif()

if(EVENTIDE_ENABLE_ZEST)
    add_subdirectory(src/zest)
endif()

if(EVENTIDE_ENABLE_TEST)
    add_subdirectory(tests)
endif()
