cmake_minimum_required(VERSION 3.16)

project(eventide LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(EVENTIDE_ENABLE_ASYNC "Enable async eventide target and libuv dependency" ON)
option(EVENTIDE_ENABLE_TEST "Build unit tests" OFF)
option(EVENTIDE_ENABLE_ZEST "Build zest test framework target" OFF)
option(EVENTIDE_SERDE_ENABLE_SIMEJSON "Enable simdjson dependency for eventide::serde" OFF)

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

# Header-only utility targets.
add_library(eventide_reflection INTERFACE)
add_library(eventide::reflection ALIAS eventide_reflection)

target_include_directories(eventide_reflection INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(eventide_serde INTERFACE)
add_library(eventide::serde ALIAS eventide_serde)

target_include_directories(eventide_serde INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eventide_serde INTERFACE
    eventide::reflection
)

if(EVENTIDE_SERDE_ENABLE_SIMEJSON)
    # Keep simdjson in "library consumer" mode.
    set(SIMDJSON_BUILD_STATIC_LIB OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_DEVELOPMENT_CHECKS OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_STATIC_REFLECTION OFF CACHE BOOL "" FORCE)

    # Lock behavior knobs so parent/toolchain cache does not accidentally change them.
    set(SIMDJSON_ENABLE_THREADS ON CACHE BOOL "" FORCE)
    set(SIMDJSON_EXCEPTIONS ON CACHE BOOL "" FORCE)
    set(SIMDJSON_DISABLE_DEPRECATED_API OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_MINUS_ZERO_AS_FLOAT OFF CACHE BOOL "" FORCE)

    # Keep development / sanitizer toggles off.
    set(SIMDJSON_CHECK_EOF OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE_UNDEFINED OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE_MEMORY OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE_THREADS OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_GLIBCXX_ASSERTIONS OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_USE_LIBCPP OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_VERBOSE_LOGGING OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SKIPUTF8VALIDATION OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(simdjson)

    target_link_libraries(eventide_serde INTERFACE
        simdjson::simdjson
    )
endif()

if(EVENTIDE_ENABLE_ASYNC)
    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG v1.x
        GIT_SHALLOW TRUE

    )
    # Configure libuv via FetchContent so no system install is required.
    if(NOT WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ASAN ON CACHE BOOL "Enable AddressSanitizer for libuv" FORCE)
    endif()
    set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LIBUV_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(LIBUV_BUILD_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libuv)

    if(WIN32)
        target_compile_definitions(uv_a PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    if(NOT MSVC AND TARGET uv_a)
        target_compile_options(uv_a PRIVATE
            "-Wno-unused-function"
            "-Wno-unused-variable"
            "-Wno-unused-but-set-variable"
            "-Wno-deprecated-declarations"
            "-Wno-missing-braces"
        )
    endif()

    file(GLOB EVENTIDE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/eventide/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/eventide/**/*.cpp"
    )

    add_library(eventide_async STATIC ${EVENTIDE_SOURCES})
    add_library(eventide::async ALIAS eventide_async)

    target_include_directories(eventide_async PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(eventide_async PUBLIC
        libuv::libuv
    )
endif()

if(EVENTIDE_ENABLE_ZEST)
    FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v1.0.4
        GIT_SHALLOW TRUE
    )
    set(CPPTRACE_DISABLE_CXX_20_MODULES ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(cpptrace)

    file(GLOB ZEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/zest/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/zest/*/*.cpp"
    )

    add_library(eventide_zest STATIC ${ZEST_SOURCES})
    add_library(eventide::zest ALIAS eventide_zest)

    target_include_directories(eventide_zest PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(eventide_zest PUBLIC
        cpptrace::cpptrace
    )
endif()

if(EVENTIDE_ENABLE_TEST)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/**/*.cpp"
    )
    if(NOT EVENTIDE_ENABLE_ASYNC)
        list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/tests/eventide/.*")
        message(STATUS "EVENTIDE_ENABLE_ASYNC=OFF: skipping tests/eventide/*")
    endif()
    add_executable(unit_tests ${TEST_SOURCES})

    target_include_directories(unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(unit_tests PRIVATE
        eventide::serde
        eventide::zest
        cpptrace::cpptrace
    )

    if(EVENTIDE_ENABLE_ASYNC)
        target_link_libraries(unit_tests PRIVATE
            eventide::async
        )
    endif()
endif()
