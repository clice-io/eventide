cmake_minimum_required(VERSION 3.16)

project(eventide LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

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
FetchContent_MakeAvailable(libuv)

add_executable(unit_tests
    "src/process.cpp"
    "src/error.cpp"
    "src/handle.cpp"
    "src/loop.cpp"
    "src/ringbuffer.cpp"
    "src/stream.cpp"
    "tests/main.cpp"
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(TARGET uv)
    target_link_libraries(unit_tests PRIVATE uv)
elseif(TARGET uv_a)
    target_link_libraries(unit_tests PRIVATE uv_a)
endif()
