FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG v1.0.4
    GIT_SHALLOW TRUE
)
set(CPPTRACE_DISABLE_CXX_20_MODULES ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpptrace)

add_library(eventide_zest STATIC)
add_library(eventide::zest ALIAS eventide_zest)

target_sources(eventide_zest PRIVATE
    "${CMAKE_SOURCE_DIR}/src/zest/runner.cpp"
)

target_include_directories(eventide_zest PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eventide_zest PUBLIC
    cpptrace::cpptrace
)
