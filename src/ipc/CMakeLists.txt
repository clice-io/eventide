add_library(eventide_ipc STATIC)
add_library(eventide::ipc ALIAS eventide_ipc)

target_sources(eventide_ipc PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/peer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/recording_transport.cpp"
)

target_include_directories(eventide_ipc PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(eventide_ipc PUBLIC
    eventide::async
)

target_compile_definitions(eventide_ipc PUBLIC
    ENABLE_JSON=$<IF:$<BOOL:${ETD_SERDE_ENABLE_SIMDJSON}>,1,0>
)

eventide_apply_project_options(eventide_ipc)

add_subdirectory(codec)
add_subdirectory(lsp)
