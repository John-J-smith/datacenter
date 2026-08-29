cmake_minimum_required(VERSION 3.16)

# Standalone layout generator: build pack tools and write *_layout.h into DC_PORT_DIR.
#
# Usage:
#   cmake -S dc/tools -B dc/tools/build -DDC_PORT_DIR=/path/to/port
#   cmake --build dc/tools/build --target gen_layouts
#
# DC_PORT_DIR must contain dc_variable_cfg.h and dc_param_cfg.h.
# Outputs: DC_PORT_DIR/dc_variable_layout.h, DC_PORT_DIR/dc_param_layout.h

project(dc_layout_gen C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(DC_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(DC_ROOT "${DC_TOOLS_DIR}/.." ABSOLUTE)

set(DC_PORT_DIR "" CACHE PATH
    "Product port directory (dc_*_cfg.h in; dc_*_layout.h out)")

if(DC_PORT_DIR STREQUAL "")
    set(_default_port "${DC_ROOT}/test/port")
    if(EXISTS "${_default_port}/dc_variable_cfg.h")
        set(DC_PORT_DIR "${_default_port}" CACHE PATH
            "Product port directory (dc_*_cfg.h in; dc_*_layout.h out)" FORCE)
        message(STATUS "DC_PORT_DIR not set; using default ${_default_port}")
    else()
        message(FATAL_ERROR
            "Set -DDC_PORT_DIR=<port_dir>\n"
            "  port_dir must contain dc_variable_cfg.h and dc_param_cfg.h.")
    endif()
endif()

get_filename_component(DC_PORT_DIR "${DC_PORT_DIR}" ABSOLUTE)

if(NOT EXISTS "${DC_PORT_DIR}/dc_variable_cfg.h")
    message(FATAL_ERROR "Missing ${DC_PORT_DIR}/dc_variable_cfg.h")
endif()
if(NOT EXISTS "${DC_PORT_DIR}/dc_param_cfg.h")
    message(FATAL_ERROR "Missing ${DC_PORT_DIR}/dc_param_cfg.h")
endif()

set(DC_VARIABLE_LAYOUT_H "${DC_PORT_DIR}/dc_variable_layout.h")
set(DC_PARAM_LAYOUT_H "${DC_PORT_DIR}/dc_param_layout.h")

message(STATUS "DC_ROOT=${DC_ROOT}")
message(STATUS "DC_PORT_DIR=${DC_PORT_DIR}")
message(STATUS "DC_VARIABLE_LAYOUT_H=${DC_VARIABLE_LAYOUT_H}")
message(STATUS "DC_PARAM_LAYOUT_H=${DC_PARAM_LAYOUT_H}")

add_executable(dc_variable_pack dc_variable_pack.c)
target_include_directories(dc_variable_pack PRIVATE
    "${DC_ROOT}/include"
    "${DC_PORT_DIR}"
)

add_executable(dc_param_pack dc_param_pack.c)
target_include_directories(dc_param_pack PRIVATE
    "${DC_ROOT}/include"
    "${DC_PORT_DIR}"
)

add_custom_target(dc_variable_layout
    COMMAND $<TARGET_FILE:dc_variable_pack> "${DC_VARIABLE_LAYOUT_H}"
    DEPENDS dc_variable_pack
            "${DC_PORT_DIR}/dc_variable_cfg.h"
            "${DC_ROOT}/include/dc_variable.h"
            "${DC_TOOLS_DIR}/dc_variable_pack.c"
    BYPRODUCTS "${DC_VARIABLE_LAYOUT_H}"
    COMMENT "Generating dc_variable_layout.h -> ${DC_PORT_DIR}"
    VERBATIM
)

add_custom_target(dc_param_layout
    COMMAND $<TARGET_FILE:dc_param_pack> "${DC_PARAM_LAYOUT_H}"
    DEPENDS dc_param_pack
            "${DC_PORT_DIR}/dc_param_cfg.h"
            "${DC_ROOT}/include/dc_param.h"
            "${DC_TOOLS_DIR}/dc_param_pack.c"
    BYPRODUCTS "${DC_PARAM_LAYOUT_H}"
    COMMENT "Generating dc_param_layout.h -> ${DC_PORT_DIR}"
    VERBATIM
)

add_custom_target(gen_layouts
    DEPENDS dc_variable_layout dc_param_layout
)

add_custom_target(dc_variable_dump
    COMMAND $<TARGET_FILE:dc_variable_pack> --dump
    DEPENDS dc_variable_pack
            "${DC_PORT_DIR}/dc_variable_cfg.h"
    COMMENT "Dump variable layout (host only)"
    VERBATIM
)

add_custom_target(dc_param_dump
    COMMAND $<TARGET_FILE:dc_param_pack> --dump
    DEPENDS dc_param_pack
            "${DC_PORT_DIR}/dc_param_cfg.h"
    COMMENT "Dump param layout (host only)"
    VERBATIM
)
