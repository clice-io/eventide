target_sources(eventide_ipc PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/bincode.cpp"
)

if(ETD_SERDE_ENABLE_SIMDJSON)
    target_sources(eventide_ipc PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/json.cpp"
    )

    target_link_libraries(eventide_ipc PUBLIC
        eventide::serde::json
    )
endif()
