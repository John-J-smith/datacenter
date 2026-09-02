/* Product param catalog and factory defaults. Edit per meter project. */
#ifndef DC_PARAM_CFG_H
#define DC_PARAM_CFG_H

#include <stdint.h>

/* Block geometry (per product). */
#define PARAM_BLOCK_BASE_LEN   (128u)
#define PARAM_BLOCK_BYTES_MAX  (64u)

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
