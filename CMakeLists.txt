cmake_minimum_required(VERSION 3.16)

project(eventide LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(EVENTIDE_ENABLE_TEST)
    if(MSVC)
        add_compile_options("/Zc:preprocessor")
    else()
        add_compile_options("-fsanitize=address" "-g")
        add_link_options("-fsanitize=address" "-g")
    endif()
    set(EVENTIDE_ENABLE_ZEST ON)
endif()

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

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
add_library(eventide STATIC ${EVENTIDE_SOURCES})
target_include_directories(eventide PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(eventide PUBLIC libuv::libuv)

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
    add_library(zest STATIC ${ZEST_SOURCES})
    target_include_directories(zest PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(zest PUBLIC cpptrace::cpptrace)
endif()

if(EVENTIDE_ENABLE_TEST)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/**/*.cpp"
    )
    add_executable(unit_tests ${TEST_SOURCES})
    target_include_directories(unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(unit_tests PRIVATE eventide zest cpptrace::cpptrace)
endif()
