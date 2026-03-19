add_library(eventide_ipc_lsp STATIC)
add_library(eventide::ipc::lsp ALIAS eventide_ipc_lsp)

target_sources(eventide_ipc_lsp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/position.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/uri.cpp"
)

target_include_directories(eventide_ipc_lsp PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(eventide_ipc_lsp PUBLIC
    eventide::ipc
)

eventide_apply_project_options(eventide_ipc_lsp)
