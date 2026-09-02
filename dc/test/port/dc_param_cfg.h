/* Product param catalog and factory defaults. Edit per meter project. */
#ifndef DC_PARAM_CFG_H
#define DC_PARAM_CFG_H

#include <stdint.h>

/* Block geometry (per product). */
#define PARAM_BLOCK_BASE_LEN   (128u)
#define PARAM_BLOCK_BYTES_MAX  (128u)

/* Subclass IDs are assigned 0..N-1 by dc_param_pack (list order). */

#define PARAM_ITEM_LIST(X) \
    PARAM_STRUCT(X, PARAM_REMOTECTRL, PARAM_STORE_FULL, 2, 4, 2) \
    PARAM_STRUCT_USE(X, PARAM_LOCALCTRL, PARAM_STORE_FULL, PARAM_REMOTECTRL) \
    PARAM_STRUCT(X, PARAM_TCP_UDP_SETUP, PARAM_STORE_ROM_EE, 5, 2, 64, 2, 1, 2) \
    PARAM_INT(X, PARAM_SEASON_SWTIME, 7u, PARAM_STORE_FULL) \
    PARAM_INT(X, PARAM_DAY_SWTIME, 7u, PARAM_STORE_FULL) \
    PARAM_INT(X, PARAM_FEE_SWTIME, 7u, PARAM_STORE_FULL) \
    PARAM_INT(X, PARAM_LADDER_SWTIME, 7u, PARAM_STORE_FULL) \
    PARAM_INT(X, PARAM_UN, 4u, PARAM_STORE_FULL) \
    PARAM_INT(X, PARAM_IB, 4u, PARAM_STORE_FULL) \
    PARAM_INT(X, PARAM_IMAX, 4u, PARAM_STORE_FULL) \
    PARAM_ARRAY(X, PARAM_HOLIDAY_DATA, 5u, 12u, PARAM_STORE_ROM_EE) \
    PARAM_LINK(X, PARAM_CALIB_DATA, 8u, 12u, PARAM_STORE_ROM_EE)

#if !defined(DC_PARAM_PACK)
PARAM_ITEM_LIST(_PARAM_ATTR_EMIT_)
#endif

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
