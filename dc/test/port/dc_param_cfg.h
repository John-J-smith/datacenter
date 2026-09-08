/* Product param catalog and factory defaults. Edit per meter project. */
#ifndef DC_PARAM_CFG_H
#define DC_PARAM_CFG_H

#include <stdint.h>
#include "dc_param_cfg_macros.h"

/* eeprom一页的大小，必须是 PARAM_BLOCK_SIZE 的整数倍 */
#define PARAM_EE_PAGE_SIZE  (128u)
/* 一个校验块的最大大小，不能跨页 */
#define PARAM_BLOCK_SIZE    (64u)
/* 编译期参数检查: PARAM_EE_PAGE_SIZE 必须是 PARAM_BLOCK_SIZE 的整数倍 */
typedef char _param_block_check[(PARAM_EE_PAGE_SIZE % PARAM_BLOCK_SIZE) ? -1 : 1];


PARAM_LIST_ATTR(PARAM_IO_RS485, PARAM_LG(3, 16, 5, 1), PARAM_LG(3, 16, 5, 1))
PARAM_LIST_ATTR(PARAM_EVENT, PARAM_LG(1, 1), PARAM_LG(1, 1), PARAM_LG(1, 1), PARAM_LG(1, 1), PARAM_LG(4, 2, 2, 4, 1))

/**
 * @brief RAM+EE双备份区参数列表
 * 参数声明方式：X 和 ST 是固定的 ，PARAM_XX 改为具体参数名
 *   INT       : PARAM_INT(X, ST, PARAM_XX, 参数长度)
 *   ARRAY     : PARAM_ARRAY(X, ST, PARAM_XX, 参数个数, 参数长度), @note 总长度 <= PARAM_BLOCK_SIZE - 2 时使用
 *   STRUCT    : PARAM_STRUCT(X, ST, PARAM_XX, 字段个数, 字段0长度, 字段1长度, ...)
 *   LIST      : PARAM_LIST(X, ST, PARAM_XX, PARAM_LG(字段个数, 字段0长度, ...), ...), @note 总长度须塞进一块
 *   LINKARRAY : PARAM_LINK(X, ST, PARAM_XX, 参数个数, 参数长度), @note 总长度 > PARAM_BLOCK_SIZE - 2 时使用
 */
#define PARAM_ITEM_LIST_RAM_EE_BK_ROWS(X, ST) \
    PARAM_INT(X, ST, PARAM_UN, 4) \
    PARAM_STRUCT(X, ST, PARAM_REMOTECTRL, 2, 4, 2) \
    PARAM_STRUCT_USE(X, ST, PARAM_LOCALCTRL, PARAM_REMOTECTRL) \
    PARAM_INT(X, ST, PARAM_SEASON_SWTIME, 7) \
    PARAM_LIST(X, ST, PARAM_LIST_DEMO, PARAM_LG(2, 2, 4), PARAM_LG(4, 2, 4, 2, 4), PARAM_LG(1, 2), PARAM_LG(1, 4)) \
    PARAM_LIST_USE(X, ST, PARAM_OVER_VOLTAGE_EVENT, PARAM_EVENT)                                                   \
    PARAM_LIST_USE(X, ST, PARAM_IO_RS485, PARAM_IO_RS485) \
    PARAM_LINK(X, ST, PARAM_LINK_TEST, 5, 30)

/**
 * @brief EE双备份区参数列表
 */
#define PARAM_ITEM_LIST_EE_BK_ROWS(X, ST) \
    PARAM_INT(X, ST, PARAM_IB, 4) \
    PARAM_STRUCT(X, ST, PARAM_TCP_UDP_SETUP, 5, 2, 32, 2, 1, 2) \
    PARAM_INT(X, ST, PARAM_DAY_SWTIME, 7) \
    PARAM_INT(X, ST, PARAM_FEE_SWTIME, 7) \
    PARAM_INT(X, ST, PARAM_LADDER_SWTIME, 7) \
    PARAM_ARRAY(X, ST, PARAM_DATA1, 5, 12) \
    PARAM_LIST_USE(X, ST, PARAM_OVER_CURRENT_EVENT, PARAM_EVENT) \
    PARAM_LINK(X, ST, PARAM_LINK_TEST2, 5, 16)

/**
 * @brief RAM+EE单备份区参数列表
 */
#define PARAM_ITEM_LIST_RAM_EE_ROWS(X, ST) \
    PARAM_INT(X, ST, PARAM_IMAX, 4) \
    PARAM_LIST_USE(X, ST, PARAM_POWER_DOWN_EVENT, PARAM_EVENT) \
    PARAM_ARRAY(X, ST, PARAM_HOLIDAY_DATA, 5, 12) \
    PARAM_LINK(X, ST, PARAM_LINK_TEST3, 5, 20) \
    PARAM_STRUCT_USE(X, ST, PARAM_TESTCTRL, PARAM_REMOTECTRL)

/**
 * @brief EE单备份区参数列表
 */
#define PARAM_ITEM_LIST_EE_ROWS(X, ST) \
    PARAM_LINK(X, ST, PARAM_CALIB_DATA, 8, 12) \
    PARAM_INT(X, ST, PARAM_TEST_IMAX, 4) \
    PARAM_ARRAY(X, ST, PARAM_TEST_DATA, 5, 12) \
    PARAM_STRUCT(X, ST, PARAM_UDP_SETUP, 4, 2, 2, 1, 2) \
    PARAM_LIST_USE(X, ST, PARAM_MAGNET_EVENT, PARAM_EVENT) 

#define PARAM_ITEM_LIST(X) \
    PARAM_ITEM_LIST_RAM_EE_BK_ROWS(X, PARAM_STORE_RAM_EE_BK) \
    PARAM_ITEM_LIST_EE_BK_ROWS(X, PARAM_STORE_EE_BK) \
    PARAM_ITEM_LIST_RAM_EE_ROWS(X, PARAM_STORE_RAM_EE) \
    PARAM_ITEM_LIST_EE_ROWS(X, PARAM_STORE_EE)

#if defined(DC_PARAM_PACK)

#define PARAM_ITEM_DEFAULTS(X) \
    X(PARAM_SEASON_SWTIME) \
    X(PARAM_DAY_SWTIME) \
    X(PARAM_FEE_SWTIME) \
    X(PARAM_LADDER_SWTIME)

static const uint8_t PARAM_SEASON_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};
static const uint8_t PARAM_DAY_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};
static const uint8_t PARAM_FEE_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};
static const uint8_t PARAM_LADDER_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};

#endif /* DC_PARAM_PACK */

#endif
