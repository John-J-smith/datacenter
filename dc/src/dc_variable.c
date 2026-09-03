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

/**
 * @brief 按变量小类 ID 查找 API 表项（ID 即表下标）
 *
 * @param subclass E_VARIABLE_TYPE 枚举值
 * @return API 表项指针；越界时返回 NULL
 */
static const ST_DC_VARIABLE_TABLE *var_find_row(uint16_t subclass)
{
    if (subclass >= tVariableApiTableCount)
    {
        return 0;
    }
    return &tVariableApiTable[subclass];
}

/**
 * @brief A 区 SRAM 头尾 magic 是否有效
 *
 * @return 非 0 表示 head/tail 均正确
 */
static int var_a_sram_ok(void)
{
    return (s_var_ram.head_a == VAR_SRAM_MAGIC_HEAD) &&
           (s_var_ram.tail_a == VAR_SRAM_MAGIC_TAIL);
}

/**
 * @brief B 区 SRAM 头尾 magic 是否有效
 *
 * @return 非 0 表示 head/tail 均正确
 */
static int var_b_sram_ok(void)
{
    return (s_var_ram.head_b == VAR_SRAM_MAGIC_HEAD) &&
           (s_var_ram.tail_b == VAR_SRAM_MAGIC_TAIL);
}

/**
 * @brief 写入 A 区 SRAM 头尾 magic（body CRC 已可信时调用）
 */
static void var_a_mark_ok(void)
{
    s_var_ram.head_a = VAR_SRAM_MAGIC_HEAD;
    s_var_ram.tail_a = VAR_SRAM_MAGIC_TAIL;
}

/**
 * @brief 写入 B 区 SRAM 头尾 magic（body CRC 已可信时调用）
 */
static void var_b_mark_ok(void)
{
    s_var_ram.head_b = VAR_SRAM_MAGIC_HEAD;
    s_var_ram.tail_b = VAR_SRAM_MAGIC_TAIL;
}

/**
 * @brief 重算并写入 A 区 body CRC
 *
 * @param body A 区 layout 体（含 payload，不含 head/tail magic）
 */
static void var_a_crc_fill(var_layout_a_t *body)
{
    body->crc = dc_crc16_ccitt((const uint8_t *)body, VAR_A_CRC_ADDR);
}

/**
 * @brief 重算并写入 B 区 body CRC
 *
 * @param body B 区 layout 体
 */
static void var_b_crc_fill(var_layout_b_t *body)
{
    body->crc = dc_crc16_ccitt((const uint8_t *)body, VAR_B_CRC_ADDR);
}

/**
 * @brief 校验 A 区 body CRC16-CCITT
 *
 * @param body A 区 layout 体
 * @return 非 0 表示 CRC 正确
 */
static int var_a_crc_ok(const var_layout_a_t *body)
{
    return body->crc == dc_crc16_ccitt((const uint8_t *)body, VAR_A_CRC_ADDR);
}

/**
 * @brief 校验 B 区 body CRC16-CCITT
 *
 * @param body B 区 layout 体
 * @return 非 0 表示 CRC 正确
 */
static int var_b_crc_ok(const var_layout_b_t *body)
{
    return body->crc == dc_crc16_ccitt((const uint8_t *)body, VAR_B_CRC_ADDR);
}

/**
 * @brief 变量 EE 备份槽绝对地址
 *
 * @param slot 备份槽枚举
 * @return 绝对 EE 地址；无效槽返回 0
 */
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

/**
 * @brief 从变量 EE 备份槽读取
 *
 * @param slot 备份槽枚举
 * @param buf  输出缓冲
 * @param len  读取字节数
 * @return 成功返回读取字节数；失败返回负错误码
 */
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

/**
 * @brief 向变量 EE 备份槽写入
 *
 * @param slot 备份槽枚举
 * @param buf  输入数据
 * @param len  写入字节数
 * @return 成功返回写入字节数；失败返回负错误码
 */
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

/**
 * @brief 尝试从指定 EE 槽恢复 A 区 body
 *
 * @param slot EE 备份槽
 * @return 非 0 表示读回且 CRC 校验通过并已写入 s_var_ram.body_a
 */
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

/**
 * @brief 尝试从指定 EE 槽恢复 B 区 body
 *
 * @param slot EE 备份槽
 * @return 非 0 表示读回且 CRC 校验通过并已写入 s_var_ram.body_b
 */
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

/**
 * @brief 判断 A 区当前是否允许写入 EE 备份
 *
 * @return 非 0 表示 magic 有效或 body CRC 正确
 */
static int var_a_backup_allowed(void)
{
    if (var_a_sram_ok())
    {
        return 1;
    }
    return var_a_crc_ok(&s_var_ram.body_a);
}

/**
 * @brief 判断 B 区当前是否允许写入 EE 备份
 *
 * @return 非 0 表示 magic 有效或 body CRC 正确
 */
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

/**
 * @brief B 区访问前完整性检查（SRAM noinit，逻辑同 A 区）
 *
 * @return 0 成功；负值表示恢复失败
 */
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

/**
 * @brief 将 A 区快照写入 PWR_ON 备份槽（双 bank 时写 0/1）
 */
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

/**
 * @brief 将 B 区快照写入 PWR_ON 备份槽（双 bank 时写 0/1）
 */
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

/**
 * @brief 将 A 区快照写入 PWR_DWN 掉电备份槽
 */
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

/**
 * @brief 将 B 区快照写入 PWR_DWN 掉电备份槽
 */
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
 * @brief 定时备份 A/B 区与掉电区
 *   A 区：达 VAR_A_BACKUP_INTERVAL_SEC 写 PWR_ON
 *   B 区：脏且达 VAR_B_BACKUP_INTERVAL_SEC 写 PWR_ON
 *   A/B：达 VAR_PWR_DWN_INTERVAL_SEC 写 PWR_DWN
 *
 * @param elapsed_sec 距上次 tick 经过的秒数
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
 * @brief 立即掉电备份 A/B 区（仅写 PWR_DWN 槽）
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

/**
 * @brief 按存储类型取得 SRAM 区基址指针
 *
 * @param stor VARIABLE_TYPEA/B/C
 * @param off  区内偏移（通常为 0）
 * @return 指向区内偏移的指针；不支持的类型返回 NULL
 */
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

/**
 * @brief D 类变量 EE 直读写（不经 SRAM 镜像）
 *
 * @param row     API 表项
 * @param rw      读缓冲（写时可为 NULL）
 * @param ro      写数据源（读时可为 NULL）
 * @param usLen   元素个数
 * @param index   起始元素索引
 * @param writing 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t var_storage_xfer(const ST_DC_VARIABLE_TABLE *row, 
                                uint8_t *rw,
                                const uint8_t *ro, 
                                uint16_t usLen, 
                                uint8_t index,
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

/**
 * @brief 变量别名读写分发（A/B/C/D 类）
 *
 * @param alias   变量别名（含小类与 index）
 * @param rw      读缓冲（写时可为 NULL）
 * @param ro      写数据源（读时可为 NULL）
 * @param usLen   元素个数（index=VAR_INDEX_ALL 时为全部分项）
 * @param type    保留，传 0
 * @param writing 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t var_xfer(uint32_t alias,
                        uint8_t *rw, 
                        const uint8_t *ro,
                        uint16_t usLen, 
                        uint8_t type, 
                        int writing)
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

/**
 * @brief 读变量（别名层 ALIAS_CLASS_VARIABLE 入口）
 *
 * @param alias    变量别名
 * @param dataPtr  输出缓冲
 * @param usLen    元素个数
 * @param type     保留，传 0
 * @return 成功返回读取字节数；失败返回负错误码
 */
int16_t dc_read_variable(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return var_xfer(alias, dataPtr, 0, usLen, type, 0);
}

/**
 * @brief 写变量（别名层 ALIAS_CLASS_VARIABLE 入口）
 *
 * @param alias    变量别名
 * @param dataPtr  输入数据
 * @param usLen    元素个数
 * @param type     保留，传 0
 * @return 成功返回写入字节数；失败返回负错误码
 */
int16_t dc_write_variable(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return var_xfer(alias, 0, dataPtr, usLen, type, 1);
}

#ifdef DC_TEST

/**
 * @brief 测试用：清空 SRAM、备份计时器与 init 标志
 */
void DcTestVarReset(void)
{
    memset(&s_var_ram, 0, sizeof s_var_ram);
    s_var_inited = 0u;
    s_b_dirty = 0u;
    s_a_pwr_on_sec = 0u;
    s_b_pwr_on_sec = 0u;
    s_pwr_dwn_sec = 0u;
}

/**
 * @brief 测试用：破坏指定区 head/tail magic
 *
 * @param zone A 或 B 区
 */
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

/**
 * @brief 测试用：破坏指定区 body CRC
 *
 * @param zone A 或 B 区
 */
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

/**
 * @brief 测试用：同时破坏 magic 与 CRC（模拟完全无效 RAM）
 *
 * @param zone A 或 B 区
 */
void DcTestVarInvalidateAll(dc_test_var_zone_t zone)
{
    DcTestVarCorruptMagic(zone);
    DcTestVarCorruptCrc(zone);
}

/**
 * @brief 测试用：重置备份计时器（PWR_ON / PWR_DWN 间隔）
 */
void DcTestVarResetBackupTimers(void)
{
    s_a_pwr_on_sec = 0u;
    s_b_pwr_on_sec = 0u;
    s_pwr_dwn_sec = 0u;
}

/**
 * @brief 测试用：查询指定区 body CRC 是否有效
 *
 * @param zone A 或 B 区
 * @return 非 0 表示 CRC 正确
 */
int DcTestVarBodyCrcOk(dc_test_var_zone_t zone)
{
    if (zone == DC_TEST_VAR_ZONE_A)
    {
        return var_a_crc_ok(&s_var_ram.body_a);
    }
    return var_b_crc_ok(&s_var_ram.body_b);
}

#endif /* DC_TEST */
