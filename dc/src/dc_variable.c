#include "dc_entry.h"
#include "datacenter.h"
#include "dc_variable.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define VAR_FIELD(tok, id, n, b) uint8_t tok[(size_t)(n) * (size_t)(b)];

typedef struct {
    VAR_LIST_A(VAR_FIELD)
    uint16_t crc;
} var_layout_a_t;

typedef struct {
    VAR_LIST_B(VAR_FIELD)
    uint16_t crc;
} var_layout_b_t;

typedef struct {
    VAR_LIST_C(VAR_FIELD)
} var_layout_c_t;

typedef struct {
    VAR_LIST_D(VAR_FIELD)
} var_layout_d_t;

#undef VAR_FIELD

#define VAR_ROW_A(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_a_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPEA },
#define VAR_ROW_B(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_b_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPEB },
#define VAR_ROW_C(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_c_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPEC },
#define VAR_ROW_D(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_d_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPED },

const STR_VARIABLE_API_TABLE tVariableApiTable[] = {
    VAR_LIST_A(VAR_ROW_A)
    VAR_LIST_B(VAR_ROW_B)
    VAR_LIST_C(VAR_ROW_C)
    VAR_LIST_D(VAR_ROW_D)
};

const uint16_t tVariableApiTableCount = (uint16_t)(sizeof(tVariableApiTable) / sizeof(tVariableApiTable[0]));

const uint16_t VAR_A_CRC_ADDR = (uint16_t)offsetof(var_layout_a_t, crc);
const uint16_t VAR_A_END_ADDR = (uint16_t)sizeof(var_layout_a_t);
const uint16_t VAR_B_CRC_ADDR = (uint16_t)offsetof(var_layout_b_t, crc);
const uint16_t VAR_B_END_ADDR = (uint16_t)sizeof(var_layout_b_t);
const uint16_t VAR_C_END_ADDR = (uint16_t)sizeof(var_layout_c_t);
const uint16_t VAR_D_END_ADDR = (uint16_t)sizeof(var_layout_d_t);

static var_layout_a_t s_ram_a;
static var_layout_b_t s_ram_b;
static var_layout_c_t s_ram_c;
static var_layout_d_t s_ram_d;

static const STR_VARIABLE_API_TABLE *var_find_row(uint16_t subclass)
{
    uint16_t i;

    for (i = 0u; i < tVariableApiTableCount; i++) {
        if (tVariableApiTable[i].eVariableType == subclass) {
            return &tVariableApiTable[i];
        }
    }
    return 0;
}

static uint8_t *var_block_base(uint8_t stor)
{
    if (stor == (uint8_t)VARIABLE_TYPEA) {
        return (uint8_t *)&s_ram_a;
    }
    if (stor == (uint8_t)VARIABLE_TYPEB) {
        return (uint8_t *)&s_ram_b;
    }
    if (stor == (uint8_t)VARIABLE_TYPEC) {
        return (uint8_t *)&s_ram_c;
    }
    if (stor == (uint8_t)VARIABLE_TYPED) {
        return (uint8_t *)&s_ram_d;
    }
    return 0;
}

static int16_t var_xfer(uint32_t genre, uint8_t *rw, const uint8_t *ro,
                        uint16_t usLen, uint8_t type, int writing)
{
    const STR_VARIABLE_API_TABLE *row;
    uint8_t *base;
    uint8_t index;
    uint16_t nbytes;
    uint16_t off;

    (void)type;

    if (usLen == 0u) {
        return 0;
    }

    row = var_find_row(ParaAliasToType(genre));
    if (row == 0) {
        return DC_RET_ALIAS_ERR;
    }

    index = GetAliasIndex(genre);
    if ((uint16_t)index + usLen > (uint16_t)row->ucIndexNum) {
        return DC_RET_PARAM_ERR;
    }

    base = var_block_base(row->ucType);
    if (base == 0) {
        return DC_RET_ALIAS_ERR;
    }

    nbytes = (uint16_t)(usLen * (uint16_t)row->ucBytes);
    off = (uint16_t)(row->eVariableAddr + (uint16_t)index * (uint16_t)row->ucBytes);

    if (writing != 0) {
        memcpy(base + off, ro, nbytes);
    } else {
        memcpy(rw, base + off, nbytes);
    }
    return (int16_t)nbytes;
}

int16_t ReadVariableData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return var_xfer(genre, dataPtr, 0, usLen, type, 0);
}

int16_t WriteVariableData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return var_xfer(genre, 0, dataPtr, usLen, type, 1);
}

#define VAR_NAME_ROW(tok, id, n, b) #tok,

static const char *const s_var_item_names[] = {
    VAR_LIST_A(VAR_NAME_ROW)
    VAR_LIST_B(VAR_NAME_ROW)
    VAR_LIST_C(VAR_NAME_ROW)
    VAR_LIST_D(VAR_NAME_ROW)
};

#undef VAR_NAME_ROW

static const char *var_stor_name(uint8_t stor)
{
    if (stor == (uint8_t)VARIABLE_TYPEA) {
        return "A";
    }
    if (stor == (uint8_t)VARIABLE_TYPEB) {
        return "B";
    }
    if (stor == (uint8_t)VARIABLE_TYPEC) {
        return "C";
    }
    if (stor == (uint8_t)VARIABLE_TYPED) {
        return "D";
    }
    return "?";
}

void VariableDumpLayout(void)
{
    uint16_t i;

    printf("=== variable blocks ===\n");
    printf("  A: ram=%p end=%u crc@%u\n",
           (void *)&s_ram_a, (unsigned)VAR_A_END_ADDR, (unsigned)VAR_A_CRC_ADDR);
    printf("  B: ram=%p end=%u crc@%u\n",
           (void *)&s_ram_b, (unsigned)VAR_B_END_ADDR, (unsigned)VAR_B_CRC_ADDR);
    printf("  C: ram=%p end=%u\n",
           (void *)&s_ram_c, (unsigned)VAR_C_END_ADDR);
    printf("  D: ram=%p end=%u\n",
           (void *)&s_ram_d, (unsigned)VAR_D_END_ADDR);

    printf("=== variable items (%u) ===\n", (unsigned)tVariableApiTableCount);
    for (i = 0u; i < tVariableApiTableCount; i++) {
        const STR_VARIABLE_API_TABLE *row = &tVariableApiTable[i];
        const char *name = (i < (uint16_t)(sizeof s_var_item_names /
                                            sizeof s_var_item_names[0]))
                               ? s_var_item_names[i]
                               : "?";

        printf("  %-24s id=0x%04X stor=%s off=%u len=%u n=%u b=%u\n",
               name,
               (unsigned)row->eVariableType,
               var_stor_name(row->ucType),
               (unsigned)row->eVariableAddr,
               (unsigned)row->ucLenth,
               (unsigned)row->ucIndexNum,
               (unsigned)row->ucBytes);
    }
}
