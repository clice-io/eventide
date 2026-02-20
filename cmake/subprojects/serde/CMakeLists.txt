add_library(eventide_serde INTERFACE)
add_library(eventide::serde ALIAS eventide_serde)

target_include_directories(eventide_serde INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eventide_serde INTERFACE
    eventide::reflection
)

if(EVENTIDE_SERDE_ENABLE_SIMDJSON)
    # Keep simdjson in "library consumer" mode.
    set(SIMDJSON_BUILD_STATIC_LIB OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_DEVELOPMENT_CHECKS OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_STATIC_REFLECTION OFF CACHE BOOL "" FORCE)

    # Lock behavior knobs so parent/toolchain cache does not accidentally change them.
    set(SIMDJSON_ENABLE_THREADS ON CACHE BOOL "" FORCE)
    set(SIMDJSON_EXCEPTIONS ON CACHE BOOL "" FORCE)
    set(SIMDJSON_DISABLE_DEPRECATED_API OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_MINUS_ZERO_AS_FLOAT OFF CACHE BOOL "" FORCE)

    # Keep development / sanitizer toggles off.
    set(SIMDJSON_CHECK_EOF OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE_UNDEFINED OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE_MEMORY OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SANITIZE_THREADS OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_GLIBCXX_ASSERTIONS OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_USE_LIBCPP OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_VERBOSE_LOGGING OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_SKIPUTF8VALIDATION OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG v4.2.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(simdjson)

    add_library(eventide_serde_json INTERFACE)
    add_library(eventide::serde::json ALIAS eventide_serde_json)

    target_include_directories(eventide_serde_json INTERFACE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(eventide_serde_json INTERFACE
        eventide::reflection
        simdjson::simdjson
    )

    target_link_libraries(eventide_serde INTERFACE
        eventide::serde::json
    )
endif()

if(EVENTIDE_SERDE_ENABLE_FLATBUFFERS)
    # flexbuffers::Reference parsing depends on symbols from flatbuffers util.cpp.
    # Build the core flatbuffers static library, skip tools/tests.
    set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "" FORCE)
    set(FLATBUFFERS_BUILD_FLATLIB ON CACHE BOOL "" FORCE)
    set(FLATBUFFERS_BUILD_SHAREDLIB OFF CACHE BOOL "" FORCE)
    set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        flatbuffers
        GIT_REPOSITORY https://github.com/google/flatbuffers.git
        GIT_TAG v25.2.10
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(flatbuffers)

    add_library(eventide_serde_flatbuffers STATIC)
    add_library(eventide::serde::flatbuffers ALIAS eventide_serde_flatbuffers)

    target_sources(eventide_serde_flatbuffers PRIVATE
        "${CMAKE_SOURCE_DIR}/src/serde/flatbuffers/flex/serializer.cpp"
        "${CMAKE_SOURCE_DIR}/src/serde/flatbuffers/flex/deserializer.cpp"
    )

    target_include_directories(eventide_serde_flatbuffers PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(eventide_serde_flatbuffers PUBLIC
        eventide::reflection
        flatbuffers
    )

    target_link_libraries(eventide_serde INTERFACE
        eventide::serde::flatbuffers
    )
endif()
