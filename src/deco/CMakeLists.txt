add_library(kota_deco_option STATIC)
add_library(kota::deco::option ALIAS kota_deco_option)
add_library(kota::option ALIAS kota_deco_option)

target_sources(kota_deco_option PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/opt_table.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/option.cc"
)

target_include_directories(kota_deco_option PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

kota_apply_project_options(kota_deco_option)

add_library(kota_deco STATIC)
add_library(kota::deco ALIAS kota_deco)

target_sources(kota_deco PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/text.cc"
)

target_include_directories(kota_deco PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(kota_deco PUBLIC
    kota::deco::option
    kota::meta
)

kota_apply_project_options(kota_deco)
