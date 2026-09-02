/* Product param catalog and factory defaults. Edit per meter project. */
#ifndef DC_PARAM_CFG_H
#define DC_PARAM_CFG_H

#include <stdint.h>

/* Subclass IDs are assigned 0..N-1 by dc_param_pack (list order). */

static const uint8_t ucCommonAttrTab[] = { DATATYPE_INT };

#define PARAM_ITEM_LIST(X) \
    X(PARAM_SEASON_SWTIME, DATATYPE_INT, 7u, (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucCommonAttrTab)

#if defined(DC_PARAM_PACK)

/* Explicit defaults only. Omit an item to fill total_len bytes with 0xFF. */
#define PARAM_ITEM_DEFAULTS(X) \
    X(PARAM_SEASON_SWTIME)

static const uint8_t PARAM_SEASON_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};

#endif /* DC_PARAM_PACK */

#endif
