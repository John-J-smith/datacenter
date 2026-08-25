#ifndef DC_VARIABLE_H
#define DC_VARIABLE_H

#include "dc_types.h"
#include "dc_variable_table.inc"
#include <stdint.h>

typedef enum {
    VARIABLE_TYPEA = 0,
    VARIABLE_TYPEB = 1,
    VARIABLE_TYPEC = 2,
    VARIABLE_TYPED = 3
} E_VARIABLE_STOR_TYPE;

#define VAR_ENUM_ROW(tok, id, n, b) tok = (id),

typedef enum {
    VAR_LIST_A(VAR_ENUM_ROW)
    VAR_LIST_B(VAR_ENUM_ROW)
    VAR_LIST_C(VAR_ENUM_ROW)
    VAR_LIST_D(VAR_ENUM_ROW)
    VARIABLE_ID_SENTINEL = 0
} E_VARIABLE_ID;

#undef VAR_ENUM_ROW

typedef struct {
    uint16_t eVariableType;
    uint16_t eVariableAddr;
    uint16_t ucLenth;
    uint8_t  ucIndexNum;
    uint8_t  ucBytes;
    uint8_t  ucType;
} STR_VARIABLE_API_TABLE;

extern const STR_VARIABLE_API_TABLE tVariableApiTable[];
extern const uint16_t tVariableApiTableCount;

extern const uint16_t VAR_A_CRC_ADDR;
extern const uint16_t VAR_A_END_ADDR;
extern const uint16_t VAR_B_CRC_ADDR;
extern const uint16_t VAR_B_END_ADDR;
extern const uint16_t VAR_C_END_ADDR;
extern const uint16_t VAR_D_END_ADDR;

void VariableDumpLayout(void);

#endif
