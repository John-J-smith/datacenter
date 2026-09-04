#ifndef DC_PARAM_H
#define DC_PARAM_H

#include <stdint.h>

#define PARAM_INDEX_ALL (0xFFu)
#define PARAM_CRC_BYTES_BLOCK (2u)

#define FLAG_SRAM (0x01u << 0)
#define FLAG_EEPROM (0x01u << 1)
#define FLAG_ROM (0x01u << 2)
#define FLAG_EEPROM_BAK (0x01u << 3)

typedef enum {
    DATATYPE_INT = 0,
    DATATYPE_ARRAY,
    DATATYPE_STRUCT,
    DATATYPE_LIST,
    DATATYPE_LINKARRAY
} E_PARAM_STORAGE_DATATYPE;

#include "dc_param_cfg_macros.h"
#include "dc_param_cfg.h"

#define PARAM_BLOCK_PAYLOAD_MAX (PARAM_BLOCK_SIZE - PARAM_CRC_BYTES_BLOCK)

#define PARAM_BLOCK_NULL_EE_OFF  (0xFFFFFFFFu)

typedef struct {
    uint32_t uBlockEeOff;
    uint8_t *ram;
    uint16_t ucBlockLen;
    uint8_t ucFlag;
} ST_PARAM_BLOCK_TABLE;

typedef struct {
    uint16_t eParamType;
    uint8_t eBlockName;
    uint8_t uParamOffset;
    uint8_t ucParamLen;
    const uint8_t *pAttr;
    const uint8_t *pDefault;
} ST_PARAM_TABLE;

extern const ST_PARAM_BLOCK_TABLE tParamBlockTable[];
extern const uint16_t tParamBlockTableCount;
extern const ST_PARAM_TABLE tParamApiTable[];
extern const uint16_t tParamApiTableCount;

extern const uint32_t PARAM_EEPROM_ORIGIN;

#endif
