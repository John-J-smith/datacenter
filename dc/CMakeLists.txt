cmake_minimum_required(VERSION 3.16)

# Standalone: open/build this directory. As subdirectory of the repo root,
# skip project() so the parent project name wins.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(dc_meter_fw C)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

add_executable(dc_param_pack tools/dc_param_pack.c)
set_property(TARGET dc_param_pack PROPERTY C_STANDARD 99)
target_include_directories(dc_param_pack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(DC_PARAM_LAYOUT_H ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param_layout.h)

add_custom_target(dc_param_layout ALL
    COMMAND $<TARGET_FILE:dc_param_pack> "${DC_PARAM_LAYOUT_H}"
    DEPENDS dc_param_pack
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param_table.inc
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param_default.inc
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param.h
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/dc_param_pack.c
    COMMENT "Generating dc_param_layout.h"
    VERBATIM
)

add_library(dc_meter_fw
    src/dc_alias.c
    src/dc_energy.c
    src/dc_demand.c
    src/dc_param.c
    src/dc_variable.c
    src/dc_list.c
    src/dc_record.c
)
target_include_directories(dc_meter_fw
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_dependencies(dc_meter_fw dc_param_layout)
set_source_files_properties(src/dc_param.c PROPERTIES OBJECT_DEPENDS ${DC_PARAM_LAYOUT_H})

add_executable(dc_fw_example main.c)
target_link_libraries(dc_fw_example PRIVATE dc_meter_fw)

add_executable(dc_param_test test_param.c)
target_link_libraries(dc_param_test PRIVATE dc_meter_fw)
