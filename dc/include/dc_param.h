#ifndef DC_PARAM_H
#define DC_PARAM_H

#include <stdint.h>

#define PARAM_INDEX_ALL (0xFFu)
#define PARAM_CRC_BYTES_BLOCK (2u)
#define PARAM_BLOCK_BASE_LEN (128u)
#define PARAM_BLOCK_BYTES_MAX (64u)
#define PARAM_BLOCK_PAYLOAD_MAX (PARAM_BLOCK_BYTES_MAX - PARAM_CRC_BYTES_BLOCK)

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

#include "dc_param_table.inc"

#define PARAM_ENUM_ROW(tok, id, dt, n, b, fl) tok = (id),

typedef enum {
    PARAM_ITEM_LIST(PARAM_ENUM_ROW)
} E_PARAMETER_TYPE;

#undef PARAM_ENUM_ROW

typedef struct {
    uint8_t eBlockName;
    uint8_t *ram;
    uint16_t ucBlockLen;
    uint8_t ucFlag;
    const uint8_t *ucPtr;
} STR_PARAM_BLOCK_TABLE;

typedef struct {
    uint16_t eParamType;
    uint8_t eBlockName;
    uint16_t uParamOffset;
    uint16_t ucParamLen;
    uint8_t ucDataType;
    uint8_t ucIndexNum;
    uint8_t ucBytes;
} STR_PARAMETER_TABLE;

extern const STR_PARAM_BLOCK_TABLE tParamBlockTable[];
extern const uint16_t tParamBlockTableCount;
extern const STR_PARAMETER_TABLE tParamApiTable[];
extern const uint16_t tParamApiTableCount;

void ParamDumpLayout(void);

#endif
