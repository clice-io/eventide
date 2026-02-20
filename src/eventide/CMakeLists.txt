FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG v1.52.0
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
    "${CMAKE_SOURCE_DIR}/src/eventide/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/eventide/**/*.cpp"
)

add_library(eventide_async STATIC ${EVENTIDE_SOURCES})
add_library(eventide::async ALIAS eventide_async)

target_include_directories(eventide_async PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eventide_async PUBLIC
    libuv::libuv
)
