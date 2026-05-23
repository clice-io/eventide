macro(bootstrap_output_dir target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
    )
endmacro()

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
bootstrap_output_dir(bootstrap_core)
add_test(NAME bootstrap_core COMMAND bootstrap_core)

add_executable(bootstrap_fixture fixture.cpp)
target_link_libraries(bootstrap_fixture PRIVATE
    kota::codec
    kota::zest
    kota::support
    cpptrace::cpptrace
)
kota_apply_project_options(bootstrap_fixture)
bootstrap_output_dir(bootstrap_fixture)

add_executable(bootstrap_runner bootstrap_runner.cpp)
kota_apply_project_options(bootstrap_runner)
bootstrap_output_dir(bootstrap_runner)
add_test(NAME bootstrap_runner
    COMMAND bootstrap_runner "$<TARGET_FILE:bootstrap_fixture>" "$<TARGET_FILE:bootstrap_fixture_focus>" "$<TARGET_FILE:bootstrap_fixture_crash>"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(bootstrap_runner PROPERTIES DEPENDS bootstrap_core)

add_executable(bootstrap_fixture_crash fixture_crash.cpp)
target_link_libraries(bootstrap_fixture_crash PRIVATE
    kota::codec
    kota::zest
    kota::support
    cpptrace::cpptrace
)
kota_apply_project_options(bootstrap_fixture_crash)
bootstrap_output_dir(bootstrap_fixture_crash)

add_executable(bootstrap_fixture_focus fixture_focus.cpp)
target_link_libraries(bootstrap_fixture_focus PRIVATE
    kota::codec
    kota::zest
    kota::support
    cpptrace::cpptrace
)
kota_apply_project_options(bootstrap_fixture_focus)
bootstrap_output_dir(bootstrap_fixture_focus)
