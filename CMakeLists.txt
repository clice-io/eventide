cmake_minimum_required(VERSION 3.16)

project(eventide LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

file(GLOB ZEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/zest/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/zest/*/*.cpp"
)

file(GLOB EVENTIDE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/eventide/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/eventide/**/*.cpp"
)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/**/*.cpp"
)

# Configure libuv via FetchContent so no system install is required.
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_STATIC ON CACHE BOOL "" FORCE)

add_compile_options("-fsanitize=address" "-g")
add_link_options("-fsanitize=address" "-g")

FetchContent_Declare(
    libuv
    URL https://dist.libuv.org/dist/v1.51.0/libuv-v1.51.0.tar.gz
)

set(CPPTRACE_DISABLE_CXX_20_MODULES ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG v1.0.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(libuv cpptrace)

add_library(zest STATIC ${ZEST_SOURCES})

target_include_directories(zest PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(eventide STATIC ${EVENTIDE_SOURCES})
target_link_libraries(zest PUBLIC cpptrace::cpptrace)

target_include_directories(eventide PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(TARGET uv)
    target_link_libraries(eventide PUBLIC uv)
elseif(TARGET uv_a)
    target_link_libraries(eventide PUBLIC uv_a)
endif()

add_executable(unit_tests ${TEST_SOURCES})

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(unit_tests PRIVATE eventide zest cpptrace::cpptrace)
