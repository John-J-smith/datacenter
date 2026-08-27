#include "dc_entry.h"
#include "datacenter.h"
#include "dc_variable.h"
#include "dc_storage_cfg.h"

#define DC_VARIABLE_LAYOUT_DEFINE
#include "dc_variable_layout.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    uint32_t head;
    var_layout_a_t body;
    uint32_t tail;
} var_sram_a_wrap_t;

typedef struct {
    uint32_t head;
    var_layout_b_t body;
    uint32_t tail;
} var_sram_b_wrap_t;

static var_sram_a_wrap_t s_sram_a;
static var_sram_b_wrap_t s_sram_b;
static var_layout_c_t s_sram_c;

static uint8_t s_var_inited;

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

static void var_sram_mark_ok(var_sram_a_wrap_t *a, var_sram_b_wrap_t *b)
{
    a->head = VAR_SRAM_MAGIC_HEAD;
    a->tail = VAR_SRAM_MAGIC_TAIL;
    b->head = VAR_SRAM_MAGIC_HEAD;
    b->tail = VAR_SRAM_MAGIC_TAIL;
}

static int var_sram_ok(const var_sram_a_wrap_t *a, const var_sram_b_wrap_t *b)
{
    return (a->head == VAR_SRAM_MAGIC_HEAD) && (a->tail == VAR_SRAM_MAGIC_TAIL) &&
           (b->head == VAR_SRAM_MAGIC_HEAD) && (b->tail == VAR_SRAM_MAGIC_TAIL);
}

static int16_t var_ee_xfer(uint32_t addr, uint8_t *rw, const uint8_t *ro,
                           uint16_t len, int writing)
{
    if (len == 0u) {
        return 0;
    }
    if (writing != 0) {
        return VAR_EE_WRITE(addr, ro, len);
    }
    return VAR_EE_READ(addr, rw, len);
}

uint32_t VariableEeSlotAddr(E_VARIABLE_EE_SLOT slot)
{
    switch (slot) {
    case VAR_EE_SLOT_A_PWR_ON_0:
        return VAR_A_EEPROM_BASE + (uint32_t)VAR_A_EE_PWR_ON_0;
    case VAR_EE_SLOT_A_PWR_ON_1:
        return VAR_A_EEPROM_BASE + (uint32_t)VAR_A_EE_PWR_ON_1;
    case VAR_EE_SLOT_A_PWR_DWN:
        return VAR_A_EEPROM_BASE + (uint32_t)VAR_A_EE_PWR_DWN;
    case VAR_EE_SLOT_B_PWR_ON_0:
        return VAR_B_EEPROM_BASE + (uint32_t)VAR_B_EE_PWR_ON_0;
    case VAR_EE_SLOT_B_PWR_ON_1:
        return VAR_B_EEPROM_BASE + (uint32_t)VAR_B_EE_PWR_ON_1;
    case VAR_EE_SLOT_B_PWR_DWN:
        return VAR_B_EEPROM_BASE + (uint32_t)VAR_B_EE_PWR_DWN;
    case VAR_EE_SLOT_D_DATA:
        return VAR_D_EEPROM_BASE;
    default:
        return 0u;
    }
}

int16_t VariableEeReadSlot(E_VARIABLE_EE_SLOT slot, uint8_t *buf, uint16_t len)
{
    uint32_t addr;

    addr = VariableEeSlotAddr(slot);
    if (addr == 0u) {
        return DC_RET_PARAM_ERR;
    }
    return var_ee_xfer(addr, buf, 0, len, 0);
}

int16_t VariableEeWriteSlot(E_VARIABLE_EE_SLOT slot, const uint8_t *buf, uint16_t len)
{
    uint32_t addr;

    addr = VariableEeSlotAddr(slot);
    if (addr == 0u) {
        return DC_RET_PARAM_ERR;
    }
    return var_ee_xfer(addr, 0, buf, len, 1);
}

static void var_restore_from_ee(void)
{
    int16_t ret;

    ret = VariableEeReadSlot(VAR_EE_SLOT_A_PWR_ON_0, (uint8_t *)&s_sram_a.body,
                             VAR_A_END_ADDR);
    if (ret == (int16_t)VAR_A_END_ADDR) {
        (void)VariableEeReadSlot(VAR_EE_SLOT_B_PWR_ON_0, (uint8_t *)&s_sram_b.body,
                                 VAR_B_END_ADDR);
    }
}

static void var_ensure_init(void)
{
    if (s_var_inited != 0u) {
        return;
    }
    memset(&s_sram_a, 0, sizeof s_sram_a);
    memset(&s_sram_b, 0, sizeof s_sram_b);
    memset(&s_sram_c, 0, sizeof s_sram_c);
    var_restore_from_ee();
    if (!var_sram_ok(&s_sram_a, &s_sram_b)) {
        var_sram_mark_ok(&s_sram_a, &s_sram_b);
    }
    s_var_inited = 1u;
}

static uint8_t *var_sram_ptr(uint8_t stor, uint16_t off)
{
    if (stor == (uint8_t)VARIABLE_TYPEA) {
        return ((uint8_t *)&s_sram_a.body) + off;
    }
    if (stor == (uint8_t)VARIABLE_TYPEB) {
        return ((uint8_t *)&s_sram_b.body) + off;
    }
    if (stor == (uint8_t)VARIABLE_TYPEC) {
        return ((uint8_t *)&s_sram_c) + off;
    }
    return 0;
}

static int16_t var_xfer_d(const STR_VARIABLE_API_TABLE *row, uint8_t *rw,
                          const uint8_t *ro, uint16_t usLen, uint8_t index,
                          int writing)
{
    uint16_t nbytes;
    uint16_t off;
    uint32_t addr;

    nbytes = (uint16_t)(usLen * (uint16_t)row->ucBytes);
    off = (uint16_t)(row->eVariableAddr + (uint16_t)index * (uint16_t)row->ucBytes);
    if ((uint32_t)off + (uint32_t)nbytes > (uint32_t)VAR_D_EE_SIZE) {
        return DC_RET_PARAM_ERR;
    }

    addr = VAR_D_EEPROM_BASE + (uint32_t)off;
    return var_ee_xfer(addr, rw, ro, nbytes, writing);
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
    var_ensure_init();

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

    if (row->ucType == (uint8_t)VARIABLE_TYPED) {
        return var_xfer_d(row, rw, ro, usLen, index, writing);
    }

    base = var_sram_ptr(row->ucType, 0u);
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
