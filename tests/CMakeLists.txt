add_executable(unit_tests)
set_target_properties(unit_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)

file(GLOB_RECURSE TEST_SOURCES RELATIVE "${PROJECT_SOURCE_DIR}" CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
    "${PROJECT_SOURCE_DIR}/tests/*.cc"
)

set(TEST_BASE_SOURCES ${TEST_SOURCES})
list(FILTER TEST_BASE_SOURCES INCLUDE REGEX "^tests/(main\\.cpp|reflection/.*\\.cpp|zest/.*\\.cpp)$")
list(TRANSFORM TEST_BASE_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
target_sources(unit_tests PRIVATE ${TEST_BASE_SOURCES})

if(EVENTIDE_ENABLE_OPTION)
    set(TEST_OPTION_SOURCES ${TEST_SOURCES})
    list(FILTER TEST_OPTION_SOURCES INCLUDE REGEX "^tests/option/.*\\.cpp$")
    list(TRANSFORM TEST_OPTION_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    target_sources(unit_tests PRIVATE
        ${TEST_OPTION_SOURCES}
    )
else()
    message(STATUS "EVENTIDE_ENABLE_OPTION=OFF: skipping tests/option/*")
endif()

if(EVENTIDE_ENABLE_DECO AND EVENTIDE_ENABLE_OPTION)
    set(TEST_DECO_SOURCES ${TEST_SOURCES})
    list(FILTER TEST_DECO_SOURCES INCLUDE REGEX "^tests/deco/.*\\.(cpp|cc)$")
    list(TRANSFORM TEST_DECO_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    target_sources(unit_tests PRIVATE
        ${TEST_DECO_SOURCES}
    )
elseif(EVENTIDE_ENABLE_DECO)
    message(STATUS "EVENTIDE_ENABLE_OPTION=OFF: skipping tests/deco/*")
else()
    message(STATUS "EVENTIDE_ENABLE_DECO=OFF: skipping tests/deco/*")
endif()

if(EVENTIDE_ENABLE_ASYNC)
    set(TEST_EVENTIDE_SOURCES ${TEST_SOURCES})
    list(FILTER TEST_EVENTIDE_SOURCES INCLUDE REGEX "^tests/eventide/.*\\.cpp$")
    list(TRANSFORM TEST_EVENTIDE_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    target_sources(unit_tests PRIVATE
        ${TEST_EVENTIDE_SOURCES}
    )
else()
    message(STATUS "EVENTIDE_ENABLE_ASYNC=OFF: skipping tests/eventide/*")
endif()

if(EVENTIDE_SERDE_ENABLE_SIMDJSON)
    set(TEST_SERDE_SIMDJSON_SOURCES ${TEST_SOURCES})
    list(FILTER TEST_SERDE_SIMDJSON_SOURCES INCLUDE REGEX "^tests/serde/simdjson/.*\\.cpp$")
    list(TRANSFORM TEST_SERDE_SIMDJSON_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    target_sources(unit_tests PRIVATE
        ${TEST_SERDE_SIMDJSON_SOURCES}
    )
else()
    message(STATUS "EVENTIDE_SERDE_ENABLE_SIMDJSON=OFF: skipping tests/serde/simdjson/*")
endif()

if(EVENTIDE_SERDE_ENABLE_FLATBUFFERS)
    set(TEST_SERDE_FLATBUFFERS_SOURCES ${TEST_SOURCES})
    list(FILTER TEST_SERDE_FLATBUFFERS_SOURCES INCLUDE REGEX "^tests/serde/flatbuffers/.*\\.cpp$")
    list(TRANSFORM TEST_SERDE_FLATBUFFERS_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    target_sources(unit_tests PRIVATE
        ${TEST_SERDE_FLATBUFFERS_SOURCES}
    )
else()
    message(STATUS "EVENTIDE_SERDE_ENABLE_FLATBUFFERS=OFF: skipping tests/serde/flatbuffers/*")
endif()

if(EVENTIDE_ENABLE_ASYNC AND EVENTIDE_SERDE_ENABLE_SIMDJSON)
    set(TEST_LANGUAGE_SOURCES ${TEST_SOURCES})
    list(FILTER TEST_LANGUAGE_SOURCES INCLUDE REGEX "^tests/language/.*\\.cpp$")
    list(TRANSFORM TEST_LANGUAGE_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    target_sources(unit_tests PRIVATE
        ${TEST_LANGUAGE_SOURCES}
    )
    target_link_libraries(unit_tests PRIVATE
        eventide::language
    )
else()
    message(STATUS "EVENTIDE_ENABLE_ASYNC=OFF or EVENTIDE_SERDE_ENABLE_SIMDJSON=OFF: skipping tests/language/*")
endif()

target_include_directories(unit_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(unit_tests PRIVATE
    eventide::serde
    eventide::zest
    cpptrace::cpptrace
)

if(EVENTIDE_ENABLE_OPTION)
    target_link_libraries(unit_tests PRIVATE
        eventide::option
    )
endif()

if(EVENTIDE_ENABLE_DECO AND EVENTIDE_ENABLE_OPTION)
    target_link_libraries(unit_tests PRIVATE
        eventide::deco
    )
endif()

if(EVENTIDE_ENABLE_ASYNC)
    target_link_libraries(unit_tests PRIVATE
        eventide::async
    )
endif()
