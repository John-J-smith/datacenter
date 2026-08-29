#include "dc_entry.h"
#include "datacenter.h"
#include "dc_variable.h"
#include "dc_storage_cfg.h"
#include "dc_crc16.h"

#define DC_VARIABLE_LAYOUT_DEFINE

#include "dc_variable_layout.h"

#include <stddef.h>
#include <string.h>

#ifdef DC_TEST
#include "dc_test_variable.h"
#endif

typedef struct
{
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

static const ST_DC_VARIABLE_TABLE *var_find_row(uint16_t subclass)
{
    if (subclass >= tVariableApiTableCount)
    {
        return 0;
    }
    return &tVariableApiTable[subclass];
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

static void var_a_crc_fill(var_layout_a_t *body)
{
    body->crc = dc_crc16_ccitt((const uint8_t *)body, VAR_A_CRC_ADDR);
}

static void var_b_crc_fill(var_layout_b_t *body)
{
    body->crc = dc_crc16_ccitt((const uint8_t *)body, VAR_B_CRC_ADDR);
}

static int var_a_crc_ok(const var_layout_a_t *body)
{
    return body->crc == dc_crc16_ccitt((const uint8_t *)body, VAR_A_CRC_ADDR);
}

static int var_b_crc_ok(const var_layout_b_t *body)
{
    return body->crc == dc_crc16_ccitt((const uint8_t *)body, VAR_B_CRC_ADDR);
}

uint32_t VariableEeSlotAddr(E_VARIABLE_EE_SLOT slot)
{
    switch (slot)
    {
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
    if (addr == 0u)
    {
        return DC_RET_PARAM_ERR;
    }
    return DC_STORAGE_READ(addr, buf, len);
}

int16_t VariableEeWriteSlot(E_VARIABLE_EE_SLOT slot, const uint8_t *buf, uint16_t len)
{
    uint32_t addr;

    addr = VariableEeSlotAddr(slot);
    if (addr == 0u)
    {
        return DC_RET_PARAM_ERR;
    }
    return DC_STORAGE_WRITE(addr, buf, len);
}

static int var_try_restore_a_slot(E_VARIABLE_EE_SLOT slot)
{
    var_layout_a_t tmp;

    if (VariableEeReadSlot(slot, (uint8_t *)&tmp, VAR_A_END_ADDR) !=
        (int16_t)VAR_A_END_ADDR)
    {
        return 0;
    }
    if (!var_a_crc_ok(&tmp))
    {
        return 0;
    }
    memcpy(&s_var_ram.body_a, &tmp, sizeof tmp);
    return 1;
}

static int var_try_restore_b_slot(E_VARIABLE_EE_SLOT slot)
{
    var_layout_b_t tmp;

    if (VariableEeReadSlot(slot, (uint8_t *)&tmp, VAR_B_END_ADDR) !=
        (int16_t)VAR_B_END_ADDR)
    {
        return 0;
    }
    if (!var_b_crc_ok(&tmp))
    {
        return 0;
    }
    memcpy(&s_var_ram.body_b, &tmp, sizeof tmp);
    return 1;
}

/**
 * @brief 运行中恢复A区数据
 *   CRC 错  → 从 EE 备份区恢复（恢复顺序：PWR_ON_0 → PWR_ON_1）
 *   CRC 对  → 只补 magic
 *   恢复失败 → DC_RET_PARAM_ERR
 * 
 * @return int 
 */
static int var_restore_a_backup(void)
{
    if (var_try_restore_a_slot(VAR_EE_SLOT_A_PWR_ON_0))
    {
        return 1;
    }
#if (VAR_EE_BACKUP_BANKS >= 2)
    return var_try_restore_a_slot(VAR_EE_SLOT_A_PWR_ON_1);
#else
    return 0;
#endif
}

/**
 * @brief 运行中恢复B区数据
 *   CRC 错   → 从 EE 备份区恢复（恢复顺序：PWR_ON_0 → PWR_ON_1）
 *   CRC 对   → 只补 magic
 *   恢复失败 → DC_RET_PARAM_ERR
 * 
 * @return int 
 */
static int var_restore_b_backup(void)
{
    if (var_try_restore_b_slot(VAR_EE_SLOT_B_PWR_ON_0))
    {
        return 1;
    }
#if (VAR_EE_BACKUP_BANKS >= 2)
    return var_try_restore_b_slot(VAR_EE_SLOT_B_PWR_ON_1);
#else
    return 0;
#endif
}

/**
 * @brief 上电恢复A区数据
 *   CRC 错   → 从 EE 备份区恢复（恢复顺序：PWR_DWN → PWR_ON_0 → PWR_ON_1）
 *   CRC 对   → 只补 magic
 *   恢复失败 → DC_RET_PARAM_ERR
 * 
 * @return int 
 */
static int var_restore_a_pwrup(void)
{
    if (var_try_restore_a_slot(VAR_EE_SLOT_A_PWR_DWN))
    {
        return 1;
    }
    return var_restore_a_backup();
}

/**
 * @brief 上电恢复B区数据
 *   CRC 错   → 从 EE 备份区恢复（恢复顺序：PWR_DWN → PWR_ON_0 → PWR_ON_1）
 *   CRC 对   → 只补 magic
 *   恢复失败 → DC_RET_PARAM_ERR
 * 
 * @return int 
 */
static int var_restore_b_pwrup(void)
{
    if (var_try_restore_b_slot(VAR_EE_SLOT_B_PWR_DWN))
    {
        return 1;
    }
    return var_restore_b_backup();
}

static int var_a_backup_allowed(void)
{
    if (var_a_sram_ok())
    {
        return 1;
    }
    return var_a_crc_ok(&s_var_ram.body_a);
}

static int var_b_backup_allowed(void)
{
    if (var_b_sram_ok())
    {
        return 1;
    }
    return var_b_crc_ok(&s_var_ram.body_b);
}

/**
 * @brief 检查A区数据是否被篡改
 *   head/tail OK → 直接访问
 *   head/tail 错 → 查 body CRC
 *    CRC 错  → 从 EE 备份区恢复（PWR_ON_0/1，不用掉电区）
 *    CRC 对  → 只补 magic
 *   恢复失败 → DC_RET_PARAM_ERR
 * 
 * @return int16_t 
 */
static int16_t var_a_prepare_access(void)
{
    if (var_a_sram_ok())
    {
        return 0;
    }
    if (!var_a_crc_ok(&s_var_ram.body_a))
    {
        if (!var_restore_a_backup())
        {
            return DC_RET_PARAM_ERR;
        }
    }
    var_a_mark_ok();
    return 0;
}

static int16_t var_b_prepare_access(void)
{
    if (var_b_sram_ok())
    {
        return 0;
    }
    if (!var_b_crc_ok(&s_var_ram.body_b))
    {
        if (!var_restore_b_backup())
        {
            return DC_RET_PARAM_ERR;
        }
    }
    var_b_mark_ok();
    return 0;
}

static void var_backup_a_pwr_on(void)
{
    var_layout_a_t snap;

    if (!var_a_backup_allowed())
    {
        return;
    }
    memcpy(&snap, &s_var_ram.body_a, sizeof(snap));
    var_a_crc_fill(&snap);
    VariableEeWriteSlot(VAR_EE_SLOT_A_PWR_ON_0, (const uint8_t *)&snap, VAR_A_END_ADDR);
#if (VAR_EE_BACKUP_BANKS >= 2)
    VariableEeWriteSlot(VAR_EE_SLOT_A_PWR_ON_1, (const uint8_t *)&snap, VAR_A_END_ADDR);
#endif
}

static void var_backup_b_pwr_on(void)
{
    var_layout_b_t snap;

    if (!var_b_backup_allowed())
    {
        return;
    }
    memcpy(&snap, &s_var_ram.body_b, sizeof(snap));
    var_b_crc_fill(&snap);
    VariableEeWriteSlot(VAR_EE_SLOT_B_PWR_ON_0, (const uint8_t *)&snap, VAR_B_END_ADDR);
#if (VAR_EE_BACKUP_BANKS >= 2)
    VariableEeWriteSlot(VAR_EE_SLOT_B_PWR_ON_1, (const uint8_t *)&snap, VAR_B_END_ADDR);
#endif
}

static void var_backup_a_pwr_dwn(void)
{
    var_layout_a_t snap;

    if (!var_a_backup_allowed())
    {
        return;
    }
    memcpy(&snap, &s_var_ram.body_a, sizeof(snap));
    var_a_crc_fill(&snap);
    VariableEeWriteSlot(VAR_EE_SLOT_A_PWR_DWN, (const uint8_t *)&snap, VAR_A_END_ADDR);
}

static void var_backup_b_pwr_dwn(void)
{
    var_layout_b_t snap;

    if (!var_b_backup_allowed())
    {
        return;
    }
    memcpy(&snap, &s_var_ram.body_b, sizeof(snap));
    var_b_crc_fill(&snap);
    VariableEeWriteSlot(VAR_EE_SLOT_B_PWR_DWN, (const uint8_t *)&snap, VAR_B_END_ADDR);
}

/**
 * @brief 定时备份A/B区数据
 * 
 * @param elapsed_sec
 */
void var_backup_tick(uint16_t elapsed_sec)
{
    var_ensure_init();

    s_a_pwr_on_sec = (uint16_t)(s_a_pwr_on_sec + elapsed_sec);
    s_b_pwr_on_sec = (uint16_t)(s_b_pwr_on_sec + elapsed_sec);
    s_pwr_dwn_sec = (uint16_t)(s_pwr_dwn_sec + elapsed_sec);

    // A区定时备份到 PWR_ON_0->PWR_ON_1
    if (s_a_pwr_on_sec >= VAR_A_BACKUP_INTERVAL_SEC)
    {
        s_a_pwr_on_sec = 0u;
        var_backup_a_pwr_on();
    }

    // B区如果脏数据，定时备份到 PWR_ON_0->PWR_ON_1
    if ((s_b_dirty != 0u) && (s_b_pwr_on_sec >= VAR_B_BACKUP_INTERVAL_SEC))
    {
        s_b_pwr_on_sec = 0u;
        s_b_dirty = 0u;
        var_backup_b_pwr_on();
    }

    // 掉电区定时备份到 PWR_DWN
    if (s_pwr_dwn_sec >= VAR_PWR_DWN_INTERVAL_SEC)
    {
        s_pwr_dwn_sec = 0u;
        var_backup_a_pwr_dwn();
        var_backup_b_pwr_dwn();
    }
}

/**
 * @brief 掉电备份A/B区数据，只写掉电区
 * 
 */
void var_backup_power_down(void)
{
    var_ensure_init();
    var_backup_a_pwr_dwn();
    var_backup_b_pwr_dwn();
}

/**
 * @brief 上电恢复A/B区数据
 *   CRC 错   → 从 EE 备份区恢复（恢复顺序：PWR_DWN → PWR_ON_0 → PWR_ON_1）
 *   CRC 对   → 只补 magic
 *   恢复失败 → DC_RET_PARAM_ERR
 */
static void var_ensure_init(void)
{
    if (s_var_inited != 0u)
    {
        return;
    }
    s_b_dirty = 0u;
    s_a_pwr_on_sec = 0u;
    s_b_pwr_on_sec = 0u;
    s_pwr_dwn_sec = 0u;
    if (!var_a_crc_ok(&s_var_ram.body_a))
    {
        var_restore_a_pwrup();
    }
    if (!var_a_sram_ok())
    {
        var_a_mark_ok();
    }
    if (!var_b_crc_ok(&s_var_ram.body_b))
    {
        var_restore_b_pwrup();
    }
    if (!var_b_sram_ok())
    {
        var_b_mark_ok();
    }
    s_var_inited = 1u;
}

static uint8_t *var_sram_ptr(uint8_t stor, uint16_t off)
{
    if (stor == (uint8_t)VARIABLE_TYPEA)
    {
        return ((uint8_t *)&s_var_ram.body_a) + off;
    }
    if (stor == (uint8_t)VARIABLE_TYPEB)
    {
        return ((uint8_t *)&s_var_ram.body_b) + off;
    }
    if (stor == (uint8_t)VARIABLE_TYPEC)
    {
        return ((uint8_t *)&s_var_ram.body_c) + off;
    }
    return 0;
}

static int16_t var_storage_xfer(const ST_DC_VARIABLE_TABLE *row, uint8_t *rw,
                                const uint8_t *ro, uint16_t usLen, uint8_t index,
                                int writing)
{
    uint16_t nbytes;
    uint16_t off;
    uint32_t addr;

    nbytes = (uint16_t)(usLen * (uint16_t)row->ucBytes);
    off = (uint16_t)(row->eVariableAddr + (uint16_t)index * (uint16_t)row->ucBytes);
    if ((uint32_t)off + (uint32_t)nbytes > (uint32_t)VAR_D_EE_SIZE)
    {
        return DC_RET_PARAM_ERR;
    }

    addr = VAR_D_EEPROM_BASE + (uint32_t)off;
    if (writing != 0)
    {
        return DC_STORAGE_WRITE(addr, ro, nbytes);
    }
    return DC_STORAGE_READ(addr, rw, nbytes);
}

static int16_t var_xfer(uint32_t alias, uint8_t *rw, const uint8_t *ro,
                        uint16_t usLen, uint8_t type, int writing)
{
    const ST_DC_VARIABLE_TABLE *row;
    uint8_t *base;
    uint8_t index;
    uint16_t nbytes;
    uint16_t off;
    int16_t ret;

    var_ensure_init();

    if (usLen == 0u)
    {
        return 0;
    }

    row = var_find_row(ParaAliasToType(alias));
    if (row == 0)
    {
        return DC_RET_ALIAS_ERR;
    }

    index = GetAliasIndex(alias);

    // index=VAR_INDEX_ALL(0xFF) 读写全部元素
    if (index == VAR_INDEX_ALL)
    {
        index = 0u;
        usLen = row->ucIndexNum;
    }

    if ((uint16_t)index + usLen > (uint16_t)row->ucIndexNum)
    {
        return DC_RET_PARAM_ERR;
    }

    if (row->ucType == (uint8_t)VARIABLE_TYPEA)
    {
        ret = var_a_prepare_access();
        if (ret != 0)
        {
            return ret;
        }
    }
    else if (row->ucType == (uint8_t)VARIABLE_TYPEB)
    {
        ret = var_b_prepare_access();
        if (ret != 0)
        {
            return ret;
        }
    }
    else if (row->ucType == (uint8_t)VARIABLE_TYPED)
    {
        return var_storage_xfer(row, rw, ro, usLen, index, writing);
    }

    base = var_sram_ptr(row->ucType, 0u);
    if (base == 0)
    {
        return DC_RET_ALIAS_ERR;
    }

    nbytes = (uint16_t)(usLen * (uint16_t)row->ucBytes);
    off = (uint16_t)(row->eVariableAddr + (uint16_t)index * (uint16_t)row->ucBytes);

    if (writing != 0)
    {
        memcpy(base + off, ro, nbytes);
        if (row->ucType == (uint8_t)VARIABLE_TYPEA)
        {
            var_a_crc_fill(&s_var_ram.body_a);
        }
        else if (row->ucType == (uint8_t)VARIABLE_TYPEB)
        {
            var_b_crc_fill(&s_var_ram.body_b);
            s_b_dirty = 1u;
        }
    }
    else
    {
        memcpy(rw, base + off, nbytes);
    }
    return (int16_t)nbytes;
}

int16_t dc_read_variable(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return var_xfer(alias, dataPtr, 0, usLen, type, 0);
}

int16_t dc_write_variable(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return var_xfer(alias, 0, dataPtr, usLen, type, 1);
}

#ifdef DC_TEST

void DcTestVarReset(void)
{
    memset(&s_var_ram, 0, sizeof s_var_ram);
    s_var_inited = 0u;
    s_b_dirty = 0u;
    s_a_pwr_on_sec = 0u;
    s_b_pwr_on_sec = 0u;
    s_pwr_dwn_sec = 0u;
}

void DcTestVarCorruptMagic(dc_test_var_zone_t zone)
{
    if (zone == DC_TEST_VAR_ZONE_A)
    {
        s_var_ram.head_a = 0u;
        s_var_ram.tail_a = 0u;
    }
    else
    {
        s_var_ram.head_b = 0u;
        s_var_ram.tail_b = 0u;
    }
}

void DcTestVarCorruptCrc(dc_test_var_zone_t zone)
{
    if (zone == DC_TEST_VAR_ZONE_A)
    {
        s_var_ram.body_a.crc = 0u;
    }
    else
    {
        s_var_ram.body_b.crc = 0u;
    }
}

void DcTestVarInvalidateAll(dc_test_var_zone_t zone)
{
    DcTestVarCorruptMagic(zone);
    DcTestVarCorruptCrc(zone);
}

void DcTestVarResetBackupTimers(void)
{
    s_a_pwr_on_sec = 0u;
    s_b_pwr_on_sec = 0u;
    s_pwr_dwn_sec = 0u;
}

int DcTestVarBodyCrcOk(dc_test_var_zone_t zone)
{
    if (zone == DC_TEST_VAR_ZONE_A)
    {
        return var_a_crc_ok(&s_var_ram.body_a);
    }
    return var_b_crc_ok(&s_var_ram.body_b);
}

#endif /* DC_TEST */
