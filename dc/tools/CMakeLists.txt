cmake_minimum_required(VERSION 3.16)

# 独立布局生成工程：只编译 pack 工具，把 *_layout.h 写入 DC_PORT_DIR。
# 与 dc/CMakeLists.txt（固件库 + 测试）分离，可在 CI/产品 port 侧单独跑。
#
# 用法:
#   cmake -S dc/tools -B dc/tools/build -DDC_PORT_DIR=/path/to/port
#   cmake --build dc/tools/build --target gen_layouts
#
# DC_PORT_DIR 须含 dc_variable_cfg.h、dc_param_cfg.h；
# 输出: DC_PORT_DIR/dc_variable_layout.h、dc_param_layout.h、dc_alias_layout.h
#       以及合并文档 dc_layout.md（各 pack upsert 自己的段）

project(dc_layout_gen C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 解析 dc/tools 与上级 dc/ 根目录（include/、test/port 等相对路径用）
get_filename_component(DC_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(DC_ROOT "${DC_TOOLS_DIR}/.." ABSOLUTE)

# 产品 port 目录：cfg 进、layout 出（可通过 -D 覆盖）
set(DC_PORT_DIR "" CACHE PATH
    "Product port directory (dc_*_cfg.h in; dc_*_layout.h out)")

# 未指定 DC_PORT_DIR 时，若存在 dc/test/port 则作为默认
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

# 启动前校验 port 里必须有 variable/param 两份 cfg
if(NOT EXISTS "${DC_PORT_DIR}/dc_variable_cfg.h")
    message(FATAL_ERROR "Missing ${DC_PORT_DIR}/dc_variable_cfg.h")
endif()
if(NOT EXISTS "${DC_PORT_DIR}/dc_param_cfg.h")
    message(FATAL_ERROR "Missing ${DC_PORT_DIR}/dc_param_cfg.h")
endif()

# 生成物路径（写回 port，供固件 include）
set(DC_VARIABLE_LAYOUT_H "${DC_PORT_DIR}/dc_variable_layout.h")
set(DC_LAYOUT_MD "${DC_PORT_DIR}/dc_layout.md")
set(DC_PARAM_LAYOUT_H "${DC_PORT_DIR}/dc_param_layout.h")
set(DC_ALIAS_LAYOUT_H "${DC_PORT_DIR}/dc_alias_layout.h")

message(STATUS "DC_ROOT=${DC_ROOT}")
message(STATUS "DC_PORT_DIR=${DC_PORT_DIR}")
message(STATUS "DC_VARIABLE_LAYOUT_H=${DC_VARIABLE_LAYOUT_H}")
message(STATUS "DC_PARAM_LAYOUT_H=${DC_PARAM_LAYOUT_H}")
message(STATUS "DC_ALIAS_LAYOUT_H=${DC_ALIAS_LAYOUT_H}")

# host 工具：读 cfg 里的 VAR_LIST_* / PARAM 表，输出 layout 头文件
add_executable(dc_variable_pack dc_variable_pack.c)
target_include_directories(dc_variable_pack PRIVATE
    "${DC_ROOT}/inc"   # dc_variable.h 等公共类型
    "${DC_PORT_DIR}"       # dc_variable_cfg.h（catalog；不含 storage）
)

add_executable(dc_param_pack dc_param_pack.c)
target_include_directories(dc_param_pack PRIVATE
    "${DC_ROOT}/inc"
    "${DC_PORT_DIR}"
)

add_executable(dc_alias_pack dc_alias_pack.c)
target_include_directories(dc_alias_pack PRIVATE
    "${DC_ROOT}/inc"
    "${DC_PORT_DIR}"
)

# 自定义目标：运行 pack，若内容有变则更新 port 下的 layout.h
add_custom_target(dc_variable_layout
    COMMAND $<TARGET_FILE:dc_variable_pack> "${DC_VARIABLE_LAYOUT_H}"
    DEPENDS dc_variable_pack
            "${DC_PORT_DIR}/dc_variable_cfg.h"
            "${DC_ROOT}/inc/dc_variable.h"
            "${DC_TOOLS_DIR}/dc_variable_pack.c"
    BYPRODUCTS "${DC_VARIABLE_LAYOUT_H}"
    COMMENT "Generating dc_variable_layout.h -> ${DC_PORT_DIR}"
    VERBATIM
)

add_custom_target(dc_param_layout
    COMMAND $<TARGET_FILE:dc_param_pack> "${DC_PARAM_LAYOUT_H}"
    DEPENDS dc_param_pack
            dc_variable_layout
            "${DC_PORT_DIR}/dc_param_cfg.h"
            "${DC_ROOT}/inc/dc_param.h"
            "${DC_TOOLS_DIR}/dc_param_pack.c"
    BYPRODUCTS "${DC_PARAM_LAYOUT_H}" "${DC_LAYOUT_MD}"
    COMMENT "Generating dc_param_layout.h -> ${DC_PORT_DIR}"
    VERBATIM
)

# 一次生成 variable + param 两份 layout
add_custom_target(dc_alias_layout
    COMMAND $<TARGET_FILE:dc_alias_pack> "${DC_ALIAS_LAYOUT_H}"
    DEPENDS dc_alias_pack
            "${DC_PORT_DIR}/dc_variable_cfg.h"
            "${DC_PORT_DIR}/dc_param_cfg.h"
            "${DC_ROOT}/inc/dc_param.h"
            "${DC_TOOLS_DIR}/dc_alias_pack.c"
    BYPRODUCTS "${DC_ALIAS_LAYOUT_H}"
    COMMENT "Generating dc_alias_layout.h -> ${DC_PORT_DIR}"
    VERBATIM
)

add_custom_target(gen_layouts
    DEPENDS dc_variable_layout dc_param_layout dc_alias_layout
)

# 仅打印布局到 stdout，不写文件（调试 / 文档对照）
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
