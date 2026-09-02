cmake_minimum_required(VERSION 3.16)

# Standalone: open/build this directory. As subdirectory of the repo root,
# skip project() so the parent project name wins.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(datacenter C CXX)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Product port directory: copy templates from port/ and customize.
set(DC_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/port" CACHE PATH
    "Port cfg headers and generated layout (storage cfg, variable cfg, *_layout.h)")

set(DC_VARIABLE_LAYOUT_H ${DC_PORT_DIR}/dc_variable_layout.h)
set(DC_PARAM_LAYOUT_H ${DC_PORT_DIR}/dc_param_layout.h)
set(DC_ALIAS_LAYOUT_H ${DC_PORT_DIR}/dc_alias_layout.h)

add_executable(dc_variable_pack tools/dc_variable_pack.c)
set_property(TARGET dc_variable_pack PROPERTY C_STANDARD 99)
target_include_directories(dc_variable_pack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DC_PORT_DIR}
)

add_custom_target(dc_variable_layout ALL
    COMMAND $<TARGET_FILE:dc_variable_pack> "${DC_VARIABLE_LAYOUT_H}"
    DEPENDS dc_variable_pack
            ${DC_PORT_DIR}/dc_variable_cfg.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_variable.h
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/dc_variable_pack.c
    BYPRODUCTS ${DC_VARIABLE_LAYOUT_H}
    COMMENT "Generating dc_variable_layout.h in port dir"
    VERBATIM
)

add_custom_target(dc_variable_dump
    COMMAND $<TARGET_FILE:dc_variable_pack> --dump
    DEPENDS dc_variable_pack
            ${DC_PORT_DIR}/dc_variable_cfg.h
    COMMENT "Dump variable layout (host only)"
    VERBATIM
)

add_executable(dc_param_pack tools/dc_param_pack.c)
set_property(TARGET dc_param_pack PROPERTY C_STANDARD 99)
target_include_directories(dc_param_pack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DC_PORT_DIR}
)

add_custom_target(dc_param_layout ALL
    COMMAND $<TARGET_FILE:dc_param_pack> "${DC_PARAM_LAYOUT_H}"
    DEPENDS dc_param_pack
            ${DC_PORT_DIR}/dc_param_cfg.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param_attr.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param_cfg_macros.h
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/dc_param_pack.c
    BYPRODUCTS ${DC_PARAM_LAYOUT_H}
    COMMENT "Generating dc_param_layout.h in port dir"
    VERBATIM
)

add_custom_target(dc_param_dump
    COMMAND $<TARGET_FILE:dc_param_pack> --dump
    DEPENDS dc_param_pack
            ${DC_PORT_DIR}/dc_param_cfg.h
    COMMENT "Dump param layout (host only)"
    VERBATIM
)

add_executable(dc_alias_pack tools/dc_alias_pack.c)
set_property(TARGET dc_alias_pack PROPERTY C_STANDARD 99)
target_include_directories(dc_alias_pack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DC_PORT_DIR}
)

add_custom_target(dc_alias_layout ALL
    COMMAND $<TARGET_FILE:dc_alias_pack> "${DC_ALIAS_LAYOUT_H}"
    DEPENDS dc_alias_pack
            ${DC_PORT_DIR}/dc_variable_cfg.h
            ${DC_PORT_DIR}/dc_param_cfg.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dc_param.h
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/dc_alias_pack.c
    BYPRODUCTS ${DC_ALIAS_LAYOUT_H}
    COMMENT "Generating dc_alias_layout.h in port dir"
    VERBATIM
)

add_library(datacenter
    src/dc_alias.c
    src/dc_crc16.c
    src/dc_energy.c
    src/dc_demand.c
    src/dc_param.c
    src/dc_variable.c
    src/dc_list.c
    src/dc_record.c
)
target_include_directories(datacenter
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
           ${DC_PORT_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_dependencies(datacenter dc_alias_layout dc_param_layout dc_variable_layout)
set_source_files_properties(src/dc_param.c PROPERTIES OBJECT_DEPENDS ${DC_PARAM_LAYOUT_H})
set_source_files_properties(src/dc_variable.c PROPERTIES OBJECT_DEPENDS ${DC_VARIABLE_LAYOUT_H})
set_source_files_properties(src/dc_alias.c PROPERTIES OBJECT_DEPENDS ${DC_ALIAS_LAYOUT_H})

option(DC_BUILD_TESTS "Build Google Test suite" ON)
if(DC_BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()
