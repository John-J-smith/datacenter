#include "dc_entry.h"
#include "datacenter.h"
#include "dc_variable.h"
#include "dc_storage_cfg.h"

#define DC_VARIABLE_LAYOUT_DEFINE
#include "dc_variable_layout.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    uint32_t head_a;
    var_layout_a_t body_a;
    uint32_t tail_a;
    uint32_t head_b;
    var_layout_b_t body_b;
    uint32_t tail_b;
    var_layout_c_t body_c;
} var_variable_ram_t;

static var_variable_ram_t s_var_ram;

static uint8_t s_var_inited;
static uint8_t s_b_dirty;
static uint16_t s_a_pwr_on_sec;
static uint16_t s_b_pwr_on_sec;
static uint16_t s_pwr_dwn_sec;

static void var_ensure_init(void);

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

static int var_a_sram_ok(void)
{
    return (s_var_ram.head_a == VAR_SRAM_MAGIC_HEAD) &&
           (s_var_ram.tail_a == VAR_SRAM_MAGIC_TAIL);
}

static int var_b_sram_ok(void)
{
    return (s_var_ram.head_b == VAR_SRAM_MAGIC_HEAD) &&
           (s_var_ram.tail_b == VAR_SRAM_MAGIC_TAIL);
}

static void var_a_mark_ok(void)
{
    s_var_ram.head_a = VAR_SRAM_MAGIC_HEAD;
    s_var_ram.tail_a = VAR_SRAM_MAGIC_TAIL;
}

static void var_b_mark_ok(void)
{
    s_var_ram.head_b = VAR_SRAM_MAGIC_HEAD;
    s_var_ram.tail_b = VAR_SRAM_MAGIC_TAIL;
}

static uint16_t var_crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t b;

    if (data == 0) {
        return crc;
    }
    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (b = 0u; b < 8u; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void var_a_crc_fill(var_layout_a_t *body)
{
    body->crc = var_crc16_ccitt((const uint8_t *)body, VAR_A_CRC_ADDR);
}

static void var_b_crc_fill(var_layout_b_t *body)
{
    body->crc = var_crc16_ccitt((const uint8_t *)body, VAR_B_CRC_ADDR);
}

static int var_a_crc_ok(const var_layout_a_t *body)
{
    return body->crc == var_crc16_ccitt((const uint8_t *)body, VAR_A_CRC_ADDR);
}

static int var_b_crc_ok(const var_layout_b_t *body)
{
    return body->crc == var_crc16_ccitt((const uint8_t *)body, VAR_B_CRC_ADDR);
}

static int16_t var_storage_xfer(uint32_t addr, uint8_t *rw, const uint8_t *ro,
                                uint16_t len, int writing)
{
    if (len == 0u) {
        return 0;
    }
    if (writing != 0) {
        return DC_STORAGE_WRITE(addr, ro, len);
    }
    return DC_STORAGE_READ(addr, rw, len);
}

uint32_t VariableEeSlotAddr(E_VARIABLE_EE_SLOT slot)
{
    switch (slot) {
    case VAR_EE_SLOT_A_PWR_ON_0:
        return VAR_A_EEPROM_BASE + (uint32_t)VAR_A_EE_PWR_ON_0;
#if (VAR_EE_BACKUP_BANKS >= 2)
    case VAR_EE_SLOT_A_PWR_ON_1:
        return VAR_A_EEPROM_BASE + (uint32_t)VAR_A_EE_PWR_ON_1;
#endif
    case VAR_EE_SLOT_A_PWR_DWN:
        return VAR_A_EEPROM_BASE + (uint32_t)VAR_A_EE_PWR_DWN;
    case VAR_EE_SLOT_B_PWR_ON_0:
        return VAR_B_EEPROM_BASE + (uint32_t)VAR_B_EE_PWR_ON_0;
#if (VAR_EE_BACKUP_BANKS >= 2)
    case VAR_EE_SLOT_B_PWR_ON_1:
        return VAR_B_EEPROM_BASE + (uint32_t)VAR_B_EE_PWR_ON_1;
#endif
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
    return DC_STORAGE_READ(addr, buf, len);
}

int16_t VariableEeWriteSlot(E_VARIABLE_EE_SLOT slot, const uint8_t *buf, uint16_t len)
{
    uint32_t addr;

    addr = VariableEeSlotAddr(slot);
    if (addr == 0u) {
        return DC_RET_PARAM_ERR;
    }
    return DC_STORAGE_WRITE(addr, buf, len);
}

static int var_try_restore_a_slot(E_VARIABLE_EE_SLOT slot)
{
    var_layout_a_t tmp;

    if (VariableEeReadSlot(slot, (uint8_t *)&tmp, VAR_A_END_ADDR) !=
        (int16_t)VAR_A_END_ADDR) {
        return 0;
    }
    if (!var_a_crc_ok(&tmp)) {
        return 0;
    }
    memcpy(&s_var_ram.body_a, &tmp, sizeof tmp);
    return 1;
}

static int var_try_restore_b_slot(E_VARIABLE_EE_SLOT slot)
{
    var_layout_b_t tmp;

    if (VariableEeReadSlot(slot, (uint8_t *)&tmp, VAR_B_END_ADDR) !=
        (int16_t)VAR_B_END_ADDR) {
        return 0;
    }
    if (!var_b_crc_ok(&tmp)) {
        return 0;
    }
    memcpy(&s_var_ram.body_b, &tmp, sizeof tmp);
    return 1;
}

static int var_restore_a_from_ee(void)
{
    if (!var_try_restore_a_slot(VAR_EE_SLOT_A_PWR_ON_0)) {
#if (VAR_EE_BACKUP_BANKS >= 2)
        if (!var_try_restore_a_slot(VAR_EE_SLOT_A_PWR_ON_1)) {
            return var_try_restore_a_slot(VAR_EE_SLOT_A_PWR_DWN);
        }
#else
        return var_try_restore_a_slot(VAR_EE_SLOT_A_PWR_DWN);
#endif
    }
    return 1;
}

static int var_restore_b_from_ee(void)
{
    if (!var_try_restore_b_slot(VAR_EE_SLOT_B_PWR_ON_0)) {
#if (VAR_EE_BACKUP_BANKS >= 2)
        if (!var_try_restore_b_slot(VAR_EE_SLOT_B_PWR_ON_1)) {
            return var_try_restore_b_slot(VAR_EE_SLOT_B_PWR_DWN);
        }
#else
        return var_try_restore_b_slot(VAR_EE_SLOT_B_PWR_DWN);
#endif
    }
    return 1;
}

static int var_a_backup_allowed(void)
{
    if (var_a_sram_ok()) {
        return 1;
    }
    return var_a_crc_ok(&s_var_ram.body_a);
}

static int var_b_backup_allowed(void)
{
    if (var_b_sram_ok()) {
        return 1;
    }
    return var_b_crc_ok(&s_var_ram.body_b);
}

/**
 * @brief 
 *   head/tail OK  → 直接访问
 *   head/tail 错 → 查 body CRC
 *    CRC 错     → 从 EE 恢复
 *    CRC 对     → 只补 magic
 *   恢复失败     → DC_RET_PARAM_ERR
 * 
 * @return int16_t 
 */
static int16_t var_a_prepare_access(void)
{
    if (var_a_sram_ok()) {
        return 0;
    }
    if (!var_a_crc_ok(&s_var_ram.body_a)) {
        if (!var_restore_a_from_ee()) {
            return DC_RET_PARAM_ERR;
        }
    }
    var_a_mark_ok();
    return 0;
}

static int16_t var_b_prepare_access(void)
{
    if (var_b_sram_ok()) {
        return 0;
    }
    if (!var_b_crc_ok(&s_var_ram.body_b)) {
        if (!var_restore_b_from_ee()) {
            return DC_RET_PARAM_ERR;
        }
    }
    var_b_mark_ok();
    return 0;
}

static void var_backup_a_pwr_on(void)
{
    var_layout_a_t snap;

    if (!var_a_backup_allowed()) {
        return;
    }
    memcpy(&snap, &s_var_ram.body_a, sizeof snap);
    var_a_crc_fill(&snap);
    (void)VariableEeWriteSlot(VAR_EE_SLOT_A_PWR_ON_0, (const uint8_t *)&snap, VAR_A_END_ADDR);
#if (VAR_EE_BACKUP_BANKS >= 2)
    (void)VariableEeWriteSlot(VAR_EE_SLOT_A_PWR_ON_1, (const uint8_t *)&snap, VAR_A_END_ADDR);
#endif
}

static void var_backup_b_pwr_on(void)
{
    var_layout_b_t snap;

    if (!var_b_backup_allowed()) {
        return;
    }
    memcpy(&snap, &s_var_ram.body_b, sizeof snap);
    var_b_crc_fill(&snap);
    (void)VariableEeWriteSlot(VAR_EE_SLOT_B_PWR_ON_0, (const uint8_t *)&snap, VAR_B_END_ADDR);
#if (VAR_EE_BACKUP_BANKS >= 2)
    (void)VariableEeWriteSlot(VAR_EE_SLOT_B_PWR_ON_1, (const uint8_t *)&snap, VAR_B_END_ADDR);
#endif
}

static void var_backup_a_pwr_dwn(void)
{
    var_layout_a_t snap;

    if (!var_a_backup_allowed()) {
        return;
    }
    memcpy(&snap, &s_var_ram.body_a, sizeof snap);
    var_a_crc_fill(&snap);
    (void)VariableEeWriteSlot(VAR_EE_SLOT_A_PWR_DWN, (const uint8_t *)&snap, VAR_A_END_ADDR);
}

static void var_backup_b_pwr_dwn(void)
{
    var_layout_b_t snap;

    if (!var_b_backup_allowed()) {
        return;
    }
    memcpy(&snap, &s_var_ram.body_b, sizeof snap);
    var_b_crc_fill(&snap);
    (void)VariableEeWriteSlot(VAR_EE_SLOT_B_PWR_DWN, (const uint8_t *)&snap, VAR_B_END_ADDR);
}

void VariableBackupTick(uint16_t elapsed_sec)
{
    var_ensure_init();

    s_a_pwr_on_sec = (uint16_t)(s_a_pwr_on_sec + elapsed_sec);
    s_b_pwr_on_sec = (uint16_t)(s_b_pwr_on_sec + elapsed_sec);
    s_pwr_dwn_sec = (uint16_t)(s_pwr_dwn_sec + elapsed_sec);

    if (s_a_pwr_on_sec >= VAR_A_BACKUP_INTERVAL_SEC) {
        s_a_pwr_on_sec = 0u;
        var_backup_a_pwr_on();
    }
    if ((s_b_dirty != 0u) && (s_b_pwr_on_sec >= VAR_B_BACKUP_INTERVAL_SEC)) {
        s_b_pwr_on_sec = 0u;
        s_b_dirty = 0u;
        var_backup_b_pwr_on();
    }
    if (s_pwr_dwn_sec >= VAR_PWR_DWN_INTERVAL_SEC) {
        s_pwr_dwn_sec = 0u;
        var_backup_a_pwr_dwn();
        var_backup_b_pwr_dwn();
    }
}

void VariableBackupPowerDown(void)
{
    var_ensure_init();
    var_backup_a_pwr_dwn();
    var_backup_b_pwr_dwn();
}

static void var_ensure_init(void)
{
    if (s_var_inited != 0u) {
        return;
    }
    memset(&s_var_ram, 0, sizeof s_var_ram);
    s_b_dirty = 0u;
    s_a_pwr_on_sec = 0u;
    s_b_pwr_on_sec = 0u;
    s_pwr_dwn_sec = 0u;
    (void)var_restore_a_from_ee();
    if (!var_a_sram_ok()) {
        var_a_mark_ok();
    }
    (void)var_restore_b_from_ee();
    if (!var_b_sram_ok()) {
        var_b_mark_ok();
    }
    s_var_inited = 1u;
}

static uint8_t *var_sram_ptr(uint8_t stor, uint16_t off)
{
    if (stor == (uint8_t)VARIABLE_TYPEA) {
        return ((uint8_t *)&s_var_ram.body_a) + off;
    }
    if (stor == (uint8_t)VARIABLE_TYPEB) {
        return ((uint8_t *)&s_var_ram.body_b) + off;
    }
    if (stor == (uint8_t)VARIABLE_TYPEC) {
        return ((uint8_t *)&s_var_ram.body_c) + off;
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
    return var_storage_xfer(addr, rw, ro, nbytes, writing);
}

static int16_t var_xfer(uint32_t genre, uint8_t *rw, const uint8_t *ro,
                        uint16_t usLen, uint8_t type, int writing)
{
    const STR_VARIABLE_API_TABLE *row;
    uint8_t *base;
    uint8_t index;
    uint16_t nbytes;
    uint16_t off;
    int16_t ret;

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

    if (row->ucType == (uint8_t)VARIABLE_TYPEA) {
        ret = var_a_prepare_access();
        if (ret != 0) {
            return ret;
        }
    } else if (row->ucType == (uint8_t)VARIABLE_TYPEB) {
        ret = var_b_prepare_access();
        if (ret != 0) {
            return ret;
        }
    } else if (row->ucType == (uint8_t)VARIABLE_TYPED) {
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
        if (row->ucType == (uint8_t)VARIABLE_TYPEB) {
            s_b_dirty = 1u;
        }
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
