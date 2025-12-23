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

set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_LAYOUT_HEADER "${GENERATED_INCLUDE_DIR}/eventide_uv_layout.hpp")

add_executable(eventide_uv_generator
    "src/generator.cpp"
)

target_include_directories(eventide_uv_generator PRIVATE
    "${libuv_SOURCE_DIR}/include"
)

add_custom_command(
    OUTPUT "${GENERATED_LAYOUT_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_INCLUDE_DIR}"
    COMMAND $<TARGET_FILE:eventide_uv_generator> "${GENERATED_LAYOUT_HEADER}"
    DEPENDS eventide_uv_generator
    COMMENT "Generating libuv layout header"
    VERBATIM
)

add_custom_target(generate_uv_layout ALL
    DEPENDS "${GENERATED_LAYOUT_HEADER}"
)

add_executable(unit_tests
    "src/handle.cpp"
    "src/loop.cpp"
    "src/ringbuffer.cpp"
    "src/stream.cpp"
    "tests/main.cpp"
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    "${GENERATED_INCLUDE_DIR}"
)

if(TARGET uv)
    target_link_libraries(eventide_uv_generator PRIVATE uv)
    target_link_libraries(unit_tests PRIVATE uv)
elseif(TARGET uv_a)
    target_link_libraries(eventide_uv_generator PRIVATE uv_a)
    target_link_libraries(unit_tests PRIVATE uv_a)
endif()

add_dependencies(unit_tests generate_uv_layout)
