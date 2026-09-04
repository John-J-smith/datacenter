/* Product param catalog and factory defaults. Edit per meter project. */
#ifndef DC_PARAM_CFG_H
#define DC_PARAM_CFG_H

#include <stdint.h>

/* eeprom一页的大小，必须是 PARAM_BLOCK_SIZE 的整数倍 */
#define PARAM_EE_PAGE_SIZE  (128u) 
/* 一个校验块的最大大小，不能跨页 */
#define PARAM_BLOCK_SIZE    (64u)  
/* 编译期参数检查: PARAM_EE_PAGE_SIZE 必须是 PARAM_BLOCK_SIZE 的整数倍 */
typedef char _param_block_check[(PARAM_EE_PAGE_SIZE % PARAM_BLOCK_SIZE) ? -1 : 1];

/* Subclass IDs are assigned 0..N-1 by dc_param_pack (list order). */

#define PARAM_ITEM_LIST(X) \
    PARAM_INT(X, PARAM_SEASON_SWTIME, 7u, PARAM_STORE_FULL)

#if !defined(DC_PARAM_PACK)
PARAM_ITEM_LIST(_PARAM_ATTR_EMIT_)
#endif

#if defined(DC_PARAM_PACK)

#define PARAM_ITEM_DEFAULTS(X) \
    X(PARAM_SEASON_SWTIME)

static const uint8_t PARAM_SEASON_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};

#endif /* DC_PARAM_PACK */

#endif
