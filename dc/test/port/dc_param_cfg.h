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

/* Subclass IDs are assigned 0..N-1 by dc_param_pack (list order). */

#define PARAM_ITEM_LIST_RAM_EE_BK(X) \
    PARAM_STRUCT(X, PARAM_REMOTECTRL, PARAM_STORE_RAM_EE_BK, 2, 4, 2) \
    PARAM_STRUCT_USE(X, PARAM_LOCALCTRL, PARAM_STORE_RAM_EE_BK, PARAM_REMOTECTRL) \
    PARAM_STRUCT(X, PARAM_TCP_UDP_SETUP, PARAM_STORE_RAM_EE_BK, 5, 2, 32, 2, 1, 2) \
    PARAM_INT(X, PARAM_SEASON_SWTIME, 7u, PARAM_STORE_RAM_EE_BK)

#define PARAM_ITEM_LIST_EE_BK(X) \
    PARAM_INT(X, PARAM_DAY_SWTIME, 7u, PARAM_STORE_EE_BK) \
    PARAM_INT(X, PARAM_FEE_SWTIME, 7u, PARAM_STORE_EE_BK) \
    PARAM_INT(X, PARAM_LADDER_SWTIME, 7u, PARAM_STORE_EE_BK)

#define PARAM_ITEM_LIST_RAM_EE(X) \
    PARAM_INT(X, PARAM_UN, 4u, PARAM_STORE_RAM_EE) \
    PARAM_INT(X, PARAM_IB, 4u, PARAM_STORE_RAM_EE) \
    PARAM_INT(X, PARAM_IMAX, 4u, PARAM_STORE_RAM_EE) \
    PARAM_ARRAY(X, PARAM_HOLIDAY_DATA, 5u, 12u, PARAM_STORE_RAM_EE)

#define PARAM_ITEM_LIST_EE(X) \
    PARAM_LINK(X, PARAM_CALIB_DATA, 8u, 12u, PARAM_STORE_EE)

#define PARAM_ITEM_LIST(X) \
    PARAM_ITEM_LIST_RAM_EE_BK(X) \
    PARAM_ITEM_LIST_EE_BK(X) \
    PARAM_ITEM_LIST_RAM_EE(X) \
    PARAM_ITEM_LIST_EE(X)

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
