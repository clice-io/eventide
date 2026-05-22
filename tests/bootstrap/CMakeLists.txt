add_executable(bootstrap_core bootstrap_core.cpp)
target_include_directories(bootstrap_core PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src/zest"
)
target_link_libraries(bootstrap_core PRIVATE
    kota::zest
    kota::support
)
kota_apply_project_options(bootstrap_core)
set_target_properties(bootstrap_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)
add_test(NAME bootstrap_core COMMAND bootstrap_core)

add_executable(bootstrap_fixture fixture.cpp)
target_link_libraries(bootstrap_fixture PRIVATE
    kota::codec
    kota::zest
    kota::support
    cpptrace::cpptrace
)
kota_apply_project_options(bootstrap_fixture)
set_target_properties(bootstrap_fixture PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)

add_executable(bootstrap_runner bootstrap_runner.cpp)
kota_apply_project_options(bootstrap_runner)
set_target_properties(bootstrap_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)
add_test(NAME bootstrap_runner
    COMMAND bootstrap_runner "$<TARGET_FILE:bootstrap_fixture>"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(bootstrap_runner PROPERTIES DEPENDS bootstrap_core)