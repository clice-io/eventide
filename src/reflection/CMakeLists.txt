add_library(eventide_reflection INTERFACE)
add_library(eventide::reflection ALIAS eventide_reflection)

target_include_directories(eventide_reflection INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)
