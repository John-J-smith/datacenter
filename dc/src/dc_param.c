#include "dc_entry.h"
#include "datacenter.h"
#include "dc_param.h"
#include "dc_param_attr.h"
#include "dc_storage_cfg.h"
#include "dc_crc16.h"

#define DC_PARAM_LAYOUT_DEFINE

#include "dc_param_layout.h"

#include <string.h>

static uint8_t s_param_inited;
static uint8_t s_param_scratch[PARAM_BLOCK_SIZE];

/**
 * @brief 按块下标查找块表项
 *
 * @param blk 块下标（与 tParamBlockTable[] 顺序一致）
 * @return 块指针；下标越界时返回 NULL
 */
static const ST_PARAM_BLOCK_TABLE *param_find_block(uint8_t blk)
{
    if ((uint16_t)blk >= tParamBlockTableCount)
    {
        return NULL;
    }
    return &tParamBlockTable[blk];
}

/**
 * @brief 块 payload 长度（不含尾部 CRC 字节）
 *
 * @param block 块表项
 * @return payload 字节数
 */
static uint16_t param_block_payload_len(const ST_PARAM_BLOCK_TABLE *block)
{
    return (uint16_t)(block->ucBlockLen - (uint16_t)PARAM_CRC_BYTES_BLOCK);
}

static uint8_t *param_block_working(const ST_PARAM_BLOCK_TABLE *block);
static void param_block_apply_defaults(uint8_t blk);

/**
 * @brief 校验工作缓冲尾部 CRC16 是否正确
 *
 * @param block 块表项
 * @return 非 0 表示 CRC 与 payload 一致
 */
static int param_block_crc_ok(const ST_PARAM_BLOCK_TABLE *block)
{
    uint16_t payload;
    uint16_t stored;
    uint16_t calc;
    const uint8_t *ram;

    if ((block == NULL) || (block->ucBlockLen < (uint16_t)PARAM_CRC_BYTES_BLOCK))
    {
        return 0;
    }
    ram = param_block_working(block);
    payload = param_block_payload_len(block);
    stored = (uint16_t)ram[payload] |
             (uint16_t)((uint16_t)ram[payload + 1u] << 8);
    calc = dc_crc16_ccitt(ram, payload);
    return stored == calc;
}

/**
 * @brief 按当前 payload 重算并写入块尾部 CRC
 *
 * @param block 块表项
 */
static void param_block_crc_fill(const ST_PARAM_BLOCK_TABLE *block)
{
    uint16_t payload;
    uint16_t crc;
    uint8_t *ram;

    if ((block == NULL) || (block->ucBlockLen < (uint16_t)PARAM_CRC_BYTES_BLOCK))
    {
        return;
    }
    ram = param_block_working(block);
    payload = param_block_payload_len(block);
    crc = dc_crc16_ccitt(ram, payload);
    ram[payload] = (uint8_t)(crc & 0xFFu);
    ram[payload + 1u] = (uint8_t)(crc >> 8);
}

/**
 * @brief 工作缓冲：有 SRAM 用块 RAM，否则用 scratch
 */
static uint8_t *param_block_working(const ST_PARAM_BLOCK_TABLE *block)
{
    if ((block == NULL) || (block->ram == NULL))
    {
        return s_param_scratch;
    }
    return block->ram;
}

/**
 * @brief 主槽绝对地址
 */
static uint32_t param_block_ee_addr(const ST_PARAM_BLOCK_TABLE *block)
{
    return PARAM_EEPROM_ORIGIN + block->uBlockEeOff;
}

/**
 * @brief 备份区 2 绝对地址（不入 table：PARAM_EE_BAK_BASE + 主槽相对偏移）
 */
static uint32_t param_block_ee_bak_addr(const ST_PARAM_BLOCK_TABLE *block)
{
    return PARAM_EEPROM_ORIGIN + (uint32_t)PARAM_EE_BAK_BASE + block->uBlockEeOff;
}

/**
 * @brief 从指定 EE 槽读入 working 并校验 CRC
 *
 * @param bak 非 0 读备份区 2
 */
static int param_block_try_restore_ee(const ST_PARAM_BLOCK_TABLE *block, int bak)
{
    uint32_t addr;
    int16_t n;
    uint8_t *ram;

    if ((block == NULL) || (block->ucBlockLen == 0u))
    {
        return 0;
    }
    if ((block->ucFlag & FLAG_EEPROM) == 0u)
    {
        return 0;
    }
    if (block->uBlockEeOff == PARAM_BLOCK_NULL_EE_OFF)
    {
        return 0;
    }
    if ((bak != 0) && ((block->ucFlag & FLAG_EEPROM_BAK) == 0u))
    {
        return 0;
    }
    ram = param_block_working(block);
    addr = (bak != 0) ? param_block_ee_bak_addr(block) : param_block_ee_addr(block);
    n = DC_STORAGE_READ(addr, ram, block->ucBlockLen);
    if (n != (int16_t)block->ucBlockLen)
    {
        return 0;
    }
    return param_block_crc_ok(block);
}

/**
 * @brief 将 working 写入主槽；双备份时再写备份区 2
 */
static void param_block_commit_ee(const ST_PARAM_BLOCK_TABLE *block)
{
    uint8_t *src;

    if ((block == NULL) || ((block->ucFlag & FLAG_EEPROM) == 0u))
    {
        return;
    }
    if (block->uBlockEeOff == PARAM_BLOCK_NULL_EE_OFF)
    {
        return;
    }
    src = param_block_working(block);
    (void)DC_STORAGE_WRITE(param_block_ee_addr(block), src, block->ucBlockLen);
    if ((block->ucFlag & FLAG_EEPROM_BAK) != 0u)
    {
        (void)DC_STORAGE_WRITE(param_block_ee_bak_addr(block), src, block->ucBlockLen);
    }
}

/**
 * @brief EE-only 块：主槽 → 备份区 2 → pDefault / 0xFF（不写 EE）
 */
static void param_block_load_ee_only(const ST_PARAM_BLOCK_TABLE *block)
{
    uint8_t blk;

    if ((block == NULL) || (block->ram != NULL))
    {
        return;
    }
    if (param_block_try_restore_ee(block, 0) != 0)
    {
        return;
    }
    if (param_block_try_restore_ee(block, 1) != 0)
    {
        return;
    }
    blk = (uint8_t)(block - &tParamBlockTable[0]);
    param_block_apply_defaults(blk);
}

/**
 * @brief 将单块恢复为默认值
 *   payload 先填 0xFF → 按 tParamApiTable.pDefault 覆盖 → 写 CRC
 *
 * @param blk 块下标
 */
static void param_block_apply_defaults(uint8_t blk)
{
    const ST_PARAM_BLOCK_TABLE *block;
    uint16_t payload;
    uint16_t i;
    uint8_t *ram;

    block = param_find_block(blk);
    if ((block == NULL) || (block->ucBlockLen < (uint16_t)PARAM_CRC_BYTES_BLOCK))
    {
        return;
    }
    payload = param_block_payload_len(block);
    ram = param_block_working(block);
    memset(ram, 0xFF, payload);
    for (i = 0u; i < tParamApiTableCount; i++)
    {
        const ST_PARAM_TABLE *item;

        item = &tParamApiTable[i];
        if ((item->eBlockName != blk) || (item->pDefault == NULL))
        {
            continue;
        }
        memcpy(ram + item->uParamOffset, item->pDefault, item->ucParamLen);
    }
    param_block_crc_fill(block);
}

/**
 * @brief 参变量块上电初始化
 *   FLAG_SRAM 且 RAM CRC 正确 → 保留 RAM
 *   否则主槽 EE → 备份区 2（FLAG_EEPROM_BAK）→ pDefault / 0xFF
 *   不写 EEPROM；仅 dc_write 路径落盘
 */
static void param_ensure_init(void)
{
    uint16_t i;

    if (s_param_inited != 0u)
    {
        return;
    }
    for (i = 0u; i < tParamBlockTableCount; i++)
    {
        const ST_PARAM_BLOCK_TABLE *block;

        block = &tParamBlockTable[i];
        if (((block->ucFlag & FLAG_SRAM) != 0u) && (block->ram != NULL) &&
            (param_block_crc_ok(block) != 0))
        {
            continue;
        }
        if (param_block_try_restore_ee(block, 0) != 0)
        {
            continue;
        }
        if (param_block_try_restore_ee(block, 1) != 0)
        {
            continue;
        }
        param_block_apply_defaults((uint8_t)i);
    }
    s_param_inited = 1u;
}

/**
 * @brief 按参变量小类 ID 查找参变量表项
 *
 * @param subclass E_PARAMETER_TYPE 枚举值
 * @return 表项指针；未找到时返回 NULL
 */
static const ST_PARAM_TABLE *param_find_item(uint16_t subclass)
{
    uint16_t i;

    for (i = 0u; i < tParamApiTableCount; i++)
    {
        if (tParamApiTable[i].eParamType == subclass)
        {
            return &tParamApiTable[i];
        }
    }
    return NULL;
}

/**
 * @brief DATATYPE_LINKARRAY 读写（跨连续物理块逻辑拼接）
 *
 * @param item    API 表项
 * @param rw      读缓冲（写时可为 NULL）
 * @param ro      写数据源（读时可为 NULL）
 * @param usLen   记录条数
 * @param index   起始记录索引
 * @param writing 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t param_xfer_link(const ST_PARAM_TABLE *item, 
                               uint8_t *rw,
                               const uint8_t *ro, 
                               uint16_t usLen, 
                               uint8_t index,
                               int writing)
{
    const uint8_t *attr;
    uint8_t k;
    uint8_t per_page;
    uint16_t i;
    uint16_t copied;
    uint8_t *ram;
    uint8_t last_page;

    attr = item->pAttr;
    k = attr[3];
    if (k == 0u)
    {
        return DC_RET_PARAM_ERR;
    }
    per_page = attr[2];
    if (per_page == 0u)
    {
        return DC_RET_PARAM_ERR;
    }

    copied = 0u;
    last_page = 0xFFu;
    ram = NULL;
    for (i = 0u; i < usLen; i++)
    {
        uint16_t rec;
        uint8_t page;
        uint8_t slot;
        const ST_PARAM_BLOCK_TABLE *block;
        uint16_t off;

        rec = (uint16_t)index + i;
        page = (uint8_t)(rec / (uint16_t)per_page);
        slot = (uint8_t)(rec % (uint16_t)per_page);
        block = param_find_block((uint8_t)(item->eBlockName + page));
        if (block == NULL)
        {
            return DC_RET_ALIAS_ERR;
        }
        if (page != last_page)
        {
            if ((writing != 0) && (last_page != 0xFFu))
            {
                const ST_PARAM_BLOCK_TABLE *prev;

                prev = param_find_block((uint8_t)(item->eBlockName + last_page));
                param_block_crc_fill(prev);
                param_block_commit_ee(prev);
            }
            param_block_load_ee_only(block);
            ram = param_block_working(block);
            last_page = page;
        }
        off = (uint16_t)slot * (uint16_t)k;
        if (writing != 0)
        {
            memcpy(ram + off, ro + copied, k);
        }
        else
        {
            memcpy(rw + copied, ram + off, k);
        }
        copied = (uint16_t)(copied + k);
    }
    if ((writing != 0) && (last_page != 0xFFu))
    {
        const ST_PARAM_BLOCK_TABLE *block;

        block = param_find_block((uint8_t)(item->eBlockName + last_page));
        param_block_crc_fill(block);
        param_block_commit_ee(block);
    }
    return (int16_t)copied;
}

/**
 * @brief DATATYPE_STRUCT 按字段索引读写
 *
 * @param item    API 表项
 * @param block   所属块
 * @param rw      读缓冲（写时可为 NULL）
 * @param ro      写数据源（读时可为 NULL）
 * @param usLen   字段个数
 * @param index   起始字段索引
 * @param writing 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t param_xfer_struct(const ST_PARAM_TABLE *item,
                                 const ST_PARAM_BLOCK_TABLE *block,
                                 uint8_t *rw, 
                                 const uint8_t *ro,
                                 uint16_t usLen, 
                                 uint8_t index, 
                                 int writing)
{
    uint16_t copied;
    uint16_t i;
    uint8_t idx;
    uint8_t *ram;

    copied = 0u;
    idx = index;
    for (i = 0u; i < usLen; i++)
    {
        uint8_t eb;
        uint16_t off;

        eb = param_attr_elem_bytes(item, idx);
        if (eb == 0u)
        {
            return DC_RET_PARAM_ERR;
        }
        off = (uint16_t)(item->uParamOffset + param_attr_struct_field_off(item, idx));
        ram = param_block_working(block);
        if (writing != 0)
        {
            memcpy(ram + off, ro + copied, eb);
        }
        else
        {
            memcpy(rw + copied, ram + off, eb);
        }
        copied = (uint16_t)(copied + eb);
        idx = (uint8_t)(idx + 1u);
    }
    if (writing != 0)
    {
        param_block_crc_fill(block);
        param_block_commit_ee(block);
    }
    return (int16_t)copied;
}

/**
 * @brief 参变量别名读写分发（INT / ARRAY / STRUCT / LINKARRAY）
 *
 * @param alias   参变量别名（含小类与 index）
 * @param rw      读缓冲（写时可为 NULL）
 * @param ro      写数据源（读时可为 NULL）
 * @param usLen   元素个数（index=PARAM_INDEX_ALL 时为全部分项）
 * @param type    保留，传 0
 * @param writing 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t param_xfer(uint32_t alias, 
                          uint8_t *rw, 
                          const uint8_t *ro,
                          uint16_t usLen, 
                          uint8_t type, 
                          int writing)
{
    const ST_PARAM_TABLE *item;
    const ST_PARAM_BLOCK_TABLE *block;
    uint8_t index;
    uint8_t dtype;
    uint8_t index_max;
    uint16_t nbytes;
    uint16_t off;
    uint8_t elem_bytes;
    uint8_t *ram;

    param_ensure_init();

    if (usLen == 0u)
    {
        return 0;
    }

    item = param_find_item(ParaAliasToType(alias));
    if (item == NULL)
    {
        return DC_RET_ALIAS_ERR;
    }

    block = param_find_block(item->eBlockName);
    if (block == NULL)
    {
        return DC_RET_ALIAS_ERR;
    }
    param_block_load_ee_only(block);
    ram = param_block_working(block);

    dtype = param_attr_type(item);
    index_max = param_attr_index_count(item);
    index = GetAliasIndex(alias);
    if (index == PARAM_INDEX_ALL)
    {
        index = 0u;
        usLen = index_max;
    }

    if (dtype == (uint8_t)DATATYPE_LIST)
    {
        return DC_RET_PARAM_ERR;
    }

    if ((uint16_t)index + usLen > (uint16_t)index_max)
    {
        return DC_RET_PARAM_ERR;
    }

    if (dtype == (uint8_t)DATATYPE_LINKARRAY)
    {
        return param_xfer_link(item, rw, ro, usLen, index, writing);
    }

    if (dtype == (uint8_t)DATATYPE_STRUCT)
    {
        return param_xfer_struct(item, block, rw, ro, usLen, index, writing);
    }

    if (dtype == (uint8_t)DATATYPE_INT)
    {
        nbytes = item->ucParamLen;
        off = item->uParamOffset;
    }
    else
    {
        elem_bytes = param_attr_elem_bytes(item, index);
        if (elem_bytes == 0u)
        {
            return DC_RET_PARAM_ERR;
        }
        nbytes = (uint16_t)(usLen * (uint16_t)elem_bytes);
        off = (uint16_t)(item->uParamOffset + (uint16_t)index * (uint16_t)elem_bytes);
    }

    if (writing != 0)
    {
        memcpy(ram + off, ro, nbytes);
        param_block_crc_fill(block);
        param_block_commit_ee(block);
    }
    else
    {
        memcpy(rw, ram + off, nbytes);
    }
    return (int16_t)nbytes;
}

/**
 * @brief 读参变量（别名层 ALIAS_CLASS_PARAMETER 入口）
 *
 * @param alias    参变量别名
 * @param dataPtr  输出缓冲
 * @param usLen    元素个数
 * @param type     保留，传 0
 * @return 成功返回读取字节数；失败返回负错误码
 */
int16_t dc_read_param(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return param_xfer(alias, dataPtr, 0, usLen, type, 0);
}

/**
 * @brief 写参变量（别名层 ALIAS_CLASS_PARAMETER 入口）
 *
 * @param alias    参变量别名
 * @param dataPtr  输入数据
 * @param usLen    元素个数
 * @param type     保留，传 0
 * @return 成功返回写入字节数；失败返回负错误码
 */
int16_t dc_write_param(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return param_xfer(alias, 0, dataPtr, usLen, type, 1);
}

#ifdef DC_TEST

/**
 * @brief 测试用：清空块 RAM 并重置 init 标志（模拟冷启动）
 */
void DcTestParamReset(void)
{
    uint16_t i;

    s_param_inited = 0u;
    for (i = 0u; i < tParamBlockTableCount; i++)
    {
        if (tParamBlockTable[i].ram != NULL)
        {
            memset(tParamBlockTable[i].ram, 0, (size_t)tParamBlockTable[i].ucBlockLen);
        }
    }
}

/**
 * @brief 测试用：仅重置 init 标志，保留 noinit RAM（模拟软复位）
 */
void DcTestParamReinit(void)
{
    s_param_inited = 0u;
}
#endif /* DC_TEST */
