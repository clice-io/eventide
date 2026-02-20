add_library(eventide_language STATIC)
add_library(eventide::language ALIAS eventide_language)

target_sources(eventide_language PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transport.cpp"
)

target_include_directories(eventide_language PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eventide_language PUBLIC
    eventide::async
    eventide::serde::json
)
