#include "dc_entry.h"
#include "datacenter.h"
#include "dc_param.h"
#include "dc_param_attr.h"
#include "dc_storage_cfg.h"
#include "dc_crc16.h"

#define DC_PARAM_LAYOUT_DEFINE

#include "dc_param_layout.h"

#include <stdint.h>
#include <string.h>

static uint8_t s_ucParamInited;
static uint8_t s_ucParamScratch[PARAM_BLOCK_SIZE];

/**
 * @brief 按块下标查找块表项
 *
 * @param ucBlk 块下标（与 tParamBlockTable[] 顺序一致）
 * @return 块指针；下标越界时返回 NULL
 */
static const ST_PARAM_BLOCK_TABLE *param_find_block(uint8_t ucBlk)
{
    if ((uint16_t)ucBlk >= tParamBlockTableCount)
    {
        return NULL;
    }
    return &tParamBlockTable[ucBlk];
}

/**
 * @brief 块 payload 长度（不含尾部 CRC 字节）
 *
 * @param pstBlock 块表项
 * @return payload 字节数
 */
static uint16_t param_block_payload_len(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    return (uint16_t)(pstBlock->usBlockLen - (uint16_t)PARAM_CRC_BYTES_BLOCK);
}

/**
 * @brief 块内 [usOff, usOff+usNbytes) 是否落在 payload 内且长度可经 int16_t 返回
 *
 * @return 0 合法；否则 DC_RET_PARAM_ERR
 */
static int16_t param_block_check_range(const ST_PARAM_BLOCK_TABLE *pstBlock, uint16_t usOff,
                                       uint16_t usNbytes)
{
    uint16_t usPayload;

    if ((pstBlock == NULL) || (usNbytes > (uint16_t)INT16_MAX))
    {
        return DC_RET_PARAM_ERR;
    }
    usPayload = param_block_payload_len(pstBlock);
    if ((uint32_t)usOff + (uint32_t)usNbytes > (uint32_t)usPayload)
    {
        return DC_RET_PARAM_ERR;
    }
    return 0;
}

static uint8_t *param_block_data_buf(const ST_PARAM_BLOCK_TABLE *pstBlock);
static void param_block_apply_defaults(uint8_t ucBlk);
static uint8_t param_block_crc_ok(const ST_PARAM_BLOCK_TABLE *pstBlock);
static uint8_t param_block_try_restore_ee(const ST_PARAM_BLOCK_TABLE *pstBlock, uint8_t ucBak);

/**
 * @brief 块是否带 SRAM 工作区
 */
static uint8_t param_block_is_sram(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    return ((pstBlock != NULL) && ((pstBlock->ucFlag & FLAG_SRAM) != 0u) &&
            (pstBlock->pucRam != NULL))
               ? 1u
               : 0u;
}

/**
 * @brief 参变量 SRAM 头尾 magic 是否有效
 */
static uint8_t param_sram_ok(void)
{
    return ((g_stParamSram.ulHead == PARAM_SRAM_MAGIC_HEAD) &&
            (g_stParamSram.ulTail == PARAM_SRAM_MAGIC_TAIL))
               ? 1u
               : 0u;
}

/**
 * @brief 写入参变量 SRAM 头尾 magic（各带 RAM 块 CRC 已可信时调用）
 */
static void param_sram_mark_ok(void)
{
    g_stParamSram.ulHead = PARAM_SRAM_MAGIC_HEAD;
    g_stParamSram.ulTail = PARAM_SRAM_MAGIC_TAIL;
}

/**
 * @brief 所有带 RAM 的块尾 CRC 是否都正确
 */
static uint8_t param_sram_all_crc_ok(void)
{
    for (uint16_t i = 0u; i < tParamBlockTableCount; i++)
    {
        const ST_PARAM_BLOCK_TABLE *pstBlock;

        pstBlock = &tParamBlockTable[i];
        if ((param_block_is_sram(pstBlock) != 0u) && (param_block_crc_ok(pstBlock) == 0u))
        {
            return 0u;
        }
    }
    return 1u;
}

/**
 * @brief 带 RAM 的块：CRC 坏则主槽 → 备份 → 默认（不写 EE）
 */
static void param_restore_all_sram(void)
{
    for (uint16_t i = 0u; i < tParamBlockTableCount; i++)
    {
        const ST_PARAM_BLOCK_TABLE *pstBlock;

        pstBlock = &tParamBlockTable[i];
        if (param_block_is_sram(pstBlock) == 0u)
        {
            continue;
        }
        if (param_block_crc_ok(pstBlock) != 0u)
        {
            continue;
        }
        if (param_block_try_restore_ee(pstBlock, 0) != 0u)
        {
            continue;
        }
        if (param_block_try_restore_ee(pstBlock, 1) != 0u)
        {
            continue;
        }
        param_block_apply_defaults((uint8_t)i);
    }
}

/**
 * @brief 运行中访问带 RAM 的块：头尾好则信 RAM，否则按块恢复并补头尾
 */
static void param_prepare_sram_access(void)
{
    if (param_sram_ok() != 0u)
    {
        return;
    }
    param_restore_all_sram();
    if (param_sram_all_crc_ok() != 0u)
    {
        param_sram_mark_ok();
    }
}

/**
 * @brief 校验工作缓冲尾部 CRC16 是否正确
 *
 * @param pstBlock 块表项
 * @return 非 0 表示 CRC 与 payload 一致
 */
static uint8_t param_block_crc_ok(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    uint16_t usPayload;
    uint16_t usStored;
    const uint8_t *pucBlkData;

    if ((pstBlock == NULL) || (pstBlock->usBlockLen < (uint16_t)PARAM_CRC_BYTES_BLOCK))
    {
        return 0;
    }
    pucBlkData = param_block_data_buf(pstBlock);
    usPayload = param_block_payload_len(pstBlock);
    usStored = (uint16_t)pucBlkData[usPayload] |
             (uint16_t)((uint16_t)pucBlkData[usPayload + 1u] << 8);
    return usStored == dc_crc16_ccitt(pucBlkData, usPayload);
}

/**
 * @brief 按当前 payload 重算并写入块尾部 CRC
 *
 * @param pstBlock 块表项
 */
static void param_block_crc_fill(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    uint16_t usPayload;
    uint16_t usCrc;
    uint8_t *pucBlkData;

    if ((pstBlock == NULL) || (pstBlock->usBlockLen < (uint16_t)PARAM_CRC_BYTES_BLOCK))
    {
        return;
    }
    pucBlkData = param_block_data_buf(pstBlock);
    usPayload = param_block_payload_len(pstBlock);
    usCrc = dc_crc16_ccitt(pucBlkData, usPayload);
    pucBlkData[usPayload] = (uint8_t)(usCrc & 0xFFu);
    pucBlkData[usPayload + 1u] = (uint8_t)(usCrc >> 8);
}

/**
 * @brief 取块数据缓冲：有 SRAM 返回块 RAM，否则返回 scratch
 */
static uint8_t *param_block_data_buf(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    if ((pstBlock == NULL) || (pstBlock->pucRam == NULL))
    {
        return s_ucParamScratch;
    }
    return pstBlock->pucRam;
}

/**
 * @brief 主槽绝对地址
 */
static uint32_t param_block_ee_addr(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    return PARAM_EEPROM_ORIGIN + pstBlock->ulBlockEeOff;
}

/**
 * @brief 备份区 2 绝对地址（不入 table：PARAM_EE_BAK_BASE + 主槽相对偏移）
 */
static uint32_t param_block_ee_bak_addr(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    return PARAM_EEPROM_ORIGIN + (uint32_t)PARAM_EE_BAK_BASE + pstBlock->ulBlockEeOff;
}

/**
 * @brief 从指定 EE 槽读入 working 并校验 CRC
 *
 * @param ucBak 非 0 读备份区 2
 */
static uint8_t param_block_try_restore_ee(const ST_PARAM_BLOCK_TABLE *pstBlock, uint8_t ucBak)
{
    uint32_t ulAddr;
    int16_t ssLen;
    uint8_t *pucBlkData;

    if ((pstBlock == NULL) || (pstBlock->usBlockLen == 0u))
    {
        return 0;
    }
    if ((pstBlock->ucFlag & FLAG_EEPROM) == 0u)
    {
        return 0;
    }
    if (pstBlock->ulBlockEeOff == PARAM_BLOCK_NULL_EE_OFF)
    {
        return 0;
    }
    if ((ucBak != 0u) && ((pstBlock->ucFlag & FLAG_EEPROM_BAK) == 0u))
    {
        return 0;
    }
    pucBlkData = param_block_data_buf(pstBlock);
    ulAddr = (ucBak != 0u) ? param_block_ee_bak_addr(pstBlock) : param_block_ee_addr(pstBlock);
    ssLen = DC_STORAGE_READ(ulAddr, pucBlkData, pstBlock->usBlockLen);
    if (ssLen != (int16_t)pstBlock->usBlockLen)
    {
        return 0;
    }
    return param_block_crc_ok(pstBlock);
}

/**
 * @brief 将 working 写入主槽；双备份时再写备份区 2
 *
 * @return 非 0 表示全部槽写入成功或无需落盘
 */
static uint8_t param_block_commit_ee(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    uint8_t *pucBlkData;
    int16_t ssLen;

    if ((pstBlock == NULL) || ((pstBlock->ucFlag & FLAG_EEPROM) == 0u))
    {
        return 1;
    }
    if (pstBlock->ulBlockEeOff == PARAM_BLOCK_NULL_EE_OFF)
    {
        return 1;
    }
    pucBlkData = param_block_data_buf(pstBlock);
    ssLen = DC_STORAGE_WRITE(param_block_ee_addr(pstBlock), pucBlkData, pstBlock->usBlockLen);
    if (ssLen != (int16_t)pstBlock->usBlockLen)
    {
        return 0;
    }
    if ((pstBlock->ucFlag & FLAG_EEPROM_BAK) != 0u)
    {
        ssLen = DC_STORAGE_WRITE(param_block_ee_bak_addr(pstBlock), pucBlkData, pstBlock->usBlockLen);
        if (ssLen != (int16_t)pstBlock->usBlockLen)
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief EE-only 块：主槽 → 备份区 2 → pucDefault / 0xFF（不写 EE）
 */
static void param_block_load_ee_only(const ST_PARAM_BLOCK_TABLE *pstBlock)
{
    uint8_t ucBlk;

    if ((pstBlock == NULL) || (pstBlock->pucRam != NULL))
    {
        return;
    }
    if (param_block_try_restore_ee(pstBlock, 0) != 0)
    {
        return;
    }
    if (param_block_try_restore_ee(pstBlock, 1) != 0)
    {
        return;
    }
    ucBlk = (uint8_t)(pstBlock - &tParamBlockTable[0]);
    param_block_apply_defaults(ucBlk);
}

/**
 * @brief 解析 LINKARRAY 的 N/M/K
 *
 * @param pstItem API 表项
 * @param pucN    子块数
 * @param pucM    每块条数
 * @param pucK    每条字节
 * @return 非 0 表示成功
 */
static uint8_t param_link_dims(const ST_PARAM_TABLE *pstItem, uint8_t *pucN, uint8_t *pucM,
                               uint8_t *pucK)
{
    const uint8_t *pucAttr;
    uint8_t ucN;
    uint8_t ucM;
    uint16_t usNrec;

    if ((pstItem == NULL) || (pstItem->pucAttr == NULL) || (pucN == NULL) || (pucM == NULL) ||
        (pucK == NULL))
    {
        return 0;
    }
    pucAttr = pstItem->pucAttr;
    if (pucAttr[0] != (uint8_t)DATATYPE_LINKARRAY)
    {
        return 0;
    }
    *pucK = pucAttr[3];
    if ((pucAttr[1] == 0u) && (pucAttr[2] == 0u))
    {
        if (param_linkarray_dims(pstItem->ucParamLen, pucAttr[3], PARAM_BLOCK_PAYLOAD_MAX, &ucN,
                                 &ucM, &usNrec) == 0)
        {
            return 0;
        }
        *pucN = ucN;
        *pucM = ucM;
        return 1;
    }
    if ((pucAttr[1] == 0u) || (pucAttr[2] == 0u) || (pucAttr[3] == 0u))
    {
        return 0;
    }
    *pucN = pucAttr[1];
    *pucM = pucAttr[2];
    return 1;
}

/**
 * @brief 将单块恢复为默认值
 *   payload 先填 0xFF → 按 tParamApiTable.pucDefault 覆盖 → 写 CRC
 *   LINKARRAY 只拷本子块切片，长度钳在 payload 内
 *
 * @param ucBlk 块下标
 */
static void param_block_apply_defaults(uint8_t ucBlk)
{
    const ST_PARAM_BLOCK_TABLE *pstBlock;
    uint16_t usPayload;
    uint8_t *pucBlkData;

    pstBlock = param_find_block(ucBlk);
    if ((pstBlock == NULL) || (pstBlock->usBlockLen < (uint16_t)PARAM_CRC_BYTES_BLOCK))
    {
        return;
    }
    usPayload = param_block_payload_len(pstBlock);
    pucBlkData = param_block_data_buf(pstBlock);
    memset(pucBlkData, 0xFF, usPayload);
    for (uint16_t i = 0u; i < tParamApiTableCount; i++)
    {
        const ST_PARAM_TABLE *pstItem;
        uint16_t usOff;
        uint16_t usCopy;
        uint16_t usSrc;
        uint8_t ucN;
        uint8_t ucM;
        uint8_t ucK;
        uint8_t ucSub;

        pstItem = &tParamApiTable[i];
        if (pstItem->pucDefault == NULL)
        {
            continue;
        }
        if (param_attr_type(pstItem) == (uint8_t)DATATYPE_LINKARRAY)
        {
            if (param_link_dims(pstItem, &ucN, &ucM, &ucK) == 0)
            {
                continue;
            }
            if (ucBlk < pstItem->ucBlockName)
            {
                continue;
            }
            ucSub = (uint8_t)(ucBlk - pstItem->ucBlockName);
            if (ucSub >= ucN)
            {
                continue;
            }
            usSrc = (uint16_t)((uint16_t)ucSub * (uint16_t)ucM * (uint16_t)ucK);
            usCopy = (uint16_t)((uint16_t)ucM * (uint16_t)ucK);
            if (usSrc >= (uint16_t)pstItem->ucParamLen)
            {
                continue;
            }
            if ((uint16_t)(usSrc + usCopy) > (uint16_t)pstItem->ucParamLen)
            {
                usCopy = (uint16_t)((uint16_t)pstItem->ucParamLen - usSrc);
            }
            usOff = pstItem->ucParamOffset;
        }
        else
        {
            if (pstItem->ucBlockName != ucBlk)
            {
                continue;
            }
            usOff = pstItem->ucParamOffset;
            usCopy = pstItem->ucParamLen;
            usSrc = 0u;
        }
        if (usOff >= usPayload)
        {
            continue;
        }
        if ((uint32_t)usOff + (uint32_t)usCopy > (uint32_t)usPayload)
        {
            usCopy = (uint16_t)(usPayload - usOff);
        }
        memcpy(pucBlkData + usOff, pstItem->pucDefault + usSrc, usCopy);
    }
    param_block_crc_fill(pstBlock);
}

/**
 * @brief 参变量 SRAM 上电初始化
 *   带 RAM：先查块尾 CRC（头尾对也不跳过）；坏则主槽 → 备份 → 默认
 *   各块 CRC 都好且头尾坏则补头尾
 *   无 RAM 块不处理，读/写时再装 scratch
 *   不写 EEPROM；仅 dc_write 路径落盘
 */
static void param_ensure_init(void)
{
    if (s_ucParamInited != 0u)
    {
        return;
    }
    param_restore_all_sram();
    if ((param_sram_all_crc_ok() != 0u) && (param_sram_ok() == 0u))
    {
        param_sram_mark_ok();
    }
    s_ucParamInited = 1u;
}

/**
 * @brief 按参变量小类 ID 查找参变量表项
 *
 * @param usSubclass E_PARAMETER_TYPE 枚举值
 * @return 表项指针；未找到时返回 NULL
 */
static const ST_PARAM_TABLE *param_find_item(uint16_t usSubclass)
{
    for (uint16_t i = 0u; i < tParamApiTableCount; i++)
    {
        if (tParamApiTable[i].usParamType == usSubclass)
        {
            return &tParamApiTable[i];
        }
    }
    return NULL;
}

/**
 * @brief DATATYPE_LINKARRAY 读写（跨连续物理块逻辑拼接）
 *
 * @param pstItem    API 表项
 * @param pucRw      读缓冲（写时可为 NULL）
 * @param pucRo      写数据源（读时可为 NULL）
 * @param usLen   记录条数
 * @param ucIndex   起始记录索引
 * @param ucWriting 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t param_xfer_link(const ST_PARAM_TABLE *pstItem, 
                               uint8_t *pucRw,
                               const uint8_t *pucRo, 
                               uint16_t usLen, 
                               uint8_t ucIndex,
                               uint8_t ucWriting)
{
    const uint8_t *pucAttr;
    uint8_t ucRecBytes;
    uint8_t ucRecPerBlk;
    uint16_t usCopied;
    uint8_t *pucBlkData;
    uint8_t ucLastSubBlk;

    pucAttr = pstItem->pucAttr;
    ucRecBytes = pucAttr[3];
    if (ucRecBytes == 0u)
    {
        return DC_RET_PARAM_ERR;
    }
    ucRecPerBlk = pucAttr[2];
    if (ucRecPerBlk == 0u)
    {
        return DC_RET_PARAM_ERR;
    }

    usCopied = 0u;
    ucLastSubBlk = 0xFFu;
    pucBlkData = NULL;
    /* 按记录切块；换子块时提交上一块并加载下一块 */
    for (uint16_t i = 0u; i < usLen; i++)
    {
        uint16_t usRec;
        uint8_t ucSubBlk;
        uint8_t ucRecInBlk;
        const ST_PARAM_BLOCK_TABLE *pstBlock;
        uint16_t usOff;

        usRec = (uint16_t)ucIndex + i;
        ucSubBlk = (uint8_t)(usRec / (uint16_t)ucRecPerBlk);
        ucRecInBlk = (uint8_t)(usRec % (uint16_t)ucRecPerBlk);
        pstBlock = param_find_block((uint8_t)(pstItem->ucBlockName + ucSubBlk));
        if (pstBlock == NULL)
        {
            return DC_RET_ALIAS_ERR;
        }
        if (ucSubBlk != ucLastSubBlk)
        {
            if ((ucWriting != 0) && (ucLastSubBlk != 0xFFu))
            {
                const ST_PARAM_BLOCK_TABLE *pstPrev;

                pstPrev = param_find_block((uint8_t)(pstItem->ucBlockName + ucLastSubBlk));
                param_block_crc_fill(pstPrev);
                if (param_block_commit_ee(pstPrev) == 0)
                {
                    return DC_RET_PARAM_ERR;
                }
            }
            param_block_load_ee_only(pstBlock);
            pucBlkData = param_block_data_buf(pstBlock);
            ucLastSubBlk = ucSubBlk;
        }
        usOff = (uint16_t)ucRecInBlk * (uint16_t)ucRecBytes;
        if (param_block_check_range(pstBlock, usOff, ucRecBytes) != 0)
        {
            return DC_RET_PARAM_ERR;
        }
        if (ucWriting != 0)
        {
            memcpy(pucBlkData + usOff, pucRo + usCopied, ucRecBytes);
        }
        else
        {
            memcpy(pucRw + usCopied, pucBlkData + usOff, ucRecBytes);
        }
    usCopied = (uint16_t)(usCopied + ucRecBytes);
    if (usCopied > (uint16_t)INT16_MAX)
    {
        return DC_RET_PARAM_ERR;
    }
    }
    if ((ucWriting != 0) && (ucLastSubBlk != 0xFFu))
    {
        const ST_PARAM_BLOCK_TABLE *pstBlock;

        pstBlock = param_find_block((uint8_t)(pstItem->ucBlockName + ucLastSubBlk));
        param_block_crc_fill(pstBlock);
        if (param_block_commit_ee(pstBlock) == 0)
        {
            return DC_RET_PARAM_ERR;
        }
    }
    return (int16_t)usCopied;
}

/**
 * @brief DATATYPE_STRUCT 按字段索引读写
 *
 * @param pstItem    API 表项
 * @param pstBlock   所属块
 * @param pucRw      读缓冲（写时可为 NULL）
 * @param pucRo      写数据源（读时可为 NULL）
 * @param usLen   字段个数
 * @param ucIndex   起始字段索引
 * @param ucWriting 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t param_xfer_struct(const ST_PARAM_TABLE *pstItem,
                                 const ST_PARAM_BLOCK_TABLE *pstBlock,
                                 uint8_t *pucRw, 
                                 const uint8_t *pucRo,
                                 uint16_t usLen, 
                                 uint8_t ucIndex, 
                                 uint8_t ucWriting)
{
    uint16_t usCopied;
    uint8_t ucIdx;
    uint8_t *pucBlkData;

    usCopied = 0u;
    ucIdx = ucIndex;
    for (uint16_t i = 0u; i < usLen; i++)
    {
        uint8_t ucFieldBytes;
        uint16_t usOff;

        ucFieldBytes = param_attr_elem_bytes(pstItem, ucIdx);
        if (ucFieldBytes == 0u)
        {
            return DC_RET_PARAM_ERR;
        }
        usOff = (uint16_t)(pstItem->ucParamOffset + param_attr_struct_field_off(pstItem, ucIdx));
        if (param_block_check_range(pstBlock, usOff, ucFieldBytes) != 0)
        {
            return DC_RET_PARAM_ERR;
        }
        pucBlkData = param_block_data_buf(pstBlock);
        if (ucWriting != 0)
        {
            memcpy(pucBlkData + usOff, pucRo + usCopied, ucFieldBytes);
        }
        else
        {
            memcpy(pucRw + usCopied, pucBlkData + usOff, ucFieldBytes);
        }
        usCopied = (uint16_t)(usCopied + ucFieldBytes);
        ucIdx = (uint8_t)(ucIdx + 1u);
    }
    if (ucWriting != 0)
    {
        param_block_crc_fill(pstBlock);
        if (param_block_commit_ee(pstBlock) == 0)
        {
            return DC_RET_PARAM_ERR;
        }
    }
    return (int16_t)usCopied;
}

/**
 * @brief DATATYPE_LIST 按 xy / xF / 0xFF 读写（单块）
 *
 * @param pstItem    API 表项
 * @param pstBlock   所属块
 * @param pucRw      读缓冲（写时可为 NULL）
 * @param pucRo      写数据源（读时可为 NULL）
 * @param usLen   非 ALL 时必须为 1
 * @param ucIndex   分项号
 * @param ucWriting 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t param_xfer_list(const ST_PARAM_TABLE *pstItem,
                               const ST_PARAM_BLOCK_TABLE *pstBlock,
                               uint8_t *pucRw,
                               const uint8_t *pucRo,
                               uint16_t usLen,
                               uint8_t ucIndex,
                               uint8_t ucWriting)
{
    uint16_t usOff;
    uint16_t usNbytes;
    uint8_t *pucBlkData;

    /* 非 ALL 时 usLen 必须为 1；按 xy/xF/ALL 取偏移与长度 */
    if ((ucIndex != PARAM_INDEX_ALL) && (usLen != 1u))
    {
        return DC_RET_PARAM_ERR;
    }
    if (param_attr_list_lookup(pstItem->pucAttr, ucIndex, &usOff, &usNbytes) == 0)
    {
        return DC_RET_PARAM_ERR;
    }
    pucBlkData = param_block_data_buf(pstBlock);
    usOff = (uint16_t)(pstItem->ucParamOffset + usOff);
    if (param_block_check_range(pstBlock, usOff, usNbytes) != 0)
    {
        return DC_RET_PARAM_ERR;
    }
    if (ucWriting != 0)
    {
        memcpy(pucBlkData + usOff, pucRo, usNbytes);
        param_block_crc_fill(pstBlock);
        if (param_block_commit_ee(pstBlock) == 0)
        {
            return DC_RET_PARAM_ERR;
        }
    }
    else
    {
        memcpy(pucRw, pucBlkData + usOff, usNbytes);
    }
    return (int16_t)usNbytes;
}

/**
 * @brief 参变量别名读写分发（INT / ARRAY / STRUCT / LIST / LINKARRAY）
 *
 * @param ulAlias   参变量别名（含小类与 ucIndex）
 * @param pucRw      读缓冲（写时可为 NULL）
 * @param pucRo      写数据源（读时可为 NULL）
 * @param usLen   元素个数（ucIndex=PARAM_INDEX_ALL 时为全部分项）
 * @param ucType    保留，传 0
 * @param ucWriting 非 0 表示写
 * @return 成功返回传输字节数；失败返回负错误码
 */
static int16_t param_xfer(uint32_t ulAlias, 
                          uint8_t *pucRw, 
                          const uint8_t *pucRo,
                          uint16_t usLen, 
                          uint8_t ucType, 
                          uint8_t ucWriting)
{
    const ST_PARAM_TABLE *pstItem;
    const ST_PARAM_BLOCK_TABLE *pstBlock;
    uint8_t ucIndex;
    uint8_t ucDtype;
    uint8_t ucIndexMax;
    uint16_t usNbytes;
    uint16_t usOff;
    uint8_t ucElemBytes;
    uint8_t *pucBlkData;

    (void)ucType;
    param_ensure_init();

    if (usLen == 0u)
    {
        return 0;
    }
    if ((ucWriting != 0) && (pucRo == 0))
    {
        return DC_RET_PARAM_ERR;
    }
    if ((ucWriting == 0) && (pucRw == 0))
    {
        return DC_RET_PARAM_ERR;
    }

    /* 查条目、加载 EE-only 工作区 */
    pstItem = param_find_item(ParaAliasToType(ulAlias));
    if (pstItem == NULL)
    {
        return DC_RET_ALIAS_ERR;
    }

    pstBlock = param_find_block(pstItem->ucBlockName);
    if (pstBlock == NULL)
    {
        return DC_RET_ALIAS_ERR;
    }
    if (param_block_is_sram(pstBlock) != 0u)
    {
        param_prepare_sram_access();
    }
    else
    {
        param_block_load_ee_only(pstBlock);
    }
    pucBlkData = param_block_data_buf(pstBlock);

    ucDtype = param_attr_type(pstItem);
    ucIndex = GetAliasIndex(ulAlias);

    /* LIST / LINKARRAY / STRUCT 走专用路径 */
    if (ucDtype == (uint8_t)DATATYPE_LIST)
    {
        return param_xfer_list(pstItem, pstBlock, pucRw, pucRo, usLen, ucIndex, ucWriting);
    }

    ucIndexMax = param_attr_index_count(pstItem);
    if (ucIndex == PARAM_INDEX_ALL)
    {
        ucIndex = 0u;
        usLen = ucIndexMax;
    }

    if ((uint16_t)ucIndex + usLen > (uint16_t)ucIndexMax)
    {
        return DC_RET_PARAM_ERR;
    }

    if (ucDtype == (uint8_t)DATATYPE_LINKARRAY)
    {
        return param_xfer_link(pstItem, pucRw, pucRo, usLen, ucIndex, ucWriting);
    }

    if (ucDtype == (uint8_t)DATATYPE_STRUCT)
    {
        return param_xfer_struct(pstItem, pstBlock, pucRw, pucRo, usLen, ucIndex, ucWriting);
    }

    /* INT 整条；ARRAY 按元素宽算偏移与长度 */
    if (ucDtype == (uint8_t)DATATYPE_INT)
    {
        usNbytes = pstItem->ucParamLen;
        usOff = pstItem->ucParamOffset;
    }
    else
    {
        ucElemBytes = param_attr_elem_bytes(pstItem, ucIndex);
        if (ucElemBytes == 0u)
        {
            return DC_RET_PARAM_ERR;
        }
        usNbytes = (uint16_t)(usLen * (uint16_t)ucElemBytes);
        usOff = (uint16_t)(pstItem->ucParamOffset + (uint16_t)ucIndex * (uint16_t)ucElemBytes);
    }

    if (param_block_check_range(pstBlock, usOff, usNbytes) != 0)
    {
        return DC_RET_PARAM_ERR;
    }

    if (ucWriting != 0)
    {
        memcpy(pucBlkData + usOff, pucRo, usNbytes);
        param_block_crc_fill(pstBlock);
        if (param_block_commit_ee(pstBlock) == 0)
        {
            return DC_RET_PARAM_ERR;
        }
    }
    else
    {
        memcpy(pucRw, pucBlkData + usOff, usNbytes);
    }
    return (int16_t)usNbytes;
}

/**
 * @brief 读参变量（别名层 ALIAS_CLASS_PARAMETER 入口）
 *
 * @param ulAlias    参变量别名
 * @param pucBuf  输出缓冲
 * @param usLen    元素个数
 * @param ucType     保留，传 0
 * @return 成功返回读取字节数；失败返回负错误码
 */
int16_t dc_read_param(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType)
{
    return param_xfer(ulAlias, pucBuf, 0, usLen, ucType, 0);
}

/**
 * @brief 写参变量（别名层 ALIAS_CLASS_PARAMETER 入口）
 *
 * @param ulAlias    参变量别名
 * @param pucBuf  输入数据
 * @param usLen    元素个数
 * @param ucType     保留，传 0
 * @return 成功返回写入字节数；失败返回负错误码
 */
int16_t dc_write_param(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType)
{
    return param_xfer(ulAlias, 0, pucBuf, usLen, ucType, 1);
}

#ifdef DC_TEST

/**
 * @brief 测试用：清空块 RAM 并重置 init 标志（模拟冷启动）
 */
void DcTestParamReset(void)
{
    s_ucParamInited = 0u;
    memset(&g_stParamSram, 0, sizeof(g_stParamSram));
}

/**
 * @brief 测试用：仅重置 init 标志，保留 noinit RAM（模拟软复位）
 */
void DcTestParamReinit(void)
{
    s_ucParamInited = 0u;
}

/**
 * @brief 测试用：仅破坏参变量 SRAM 头尾 magic
 */
void DcTestParamCorruptSramMagic(void)
{
    g_stParamSram.ulHead = 0u;
    g_stParamSram.ulTail = 0u;
}

/**
 * @brief 测试用：翻转指定块末尾 CRC 高字节
 */
void DcTestParamCorruptBlockCrc(uint8_t ucBlk)
{
    const ST_PARAM_BLOCK_TABLE *pstBlock;
    uint16_t usCrcOff;

    pstBlock = param_find_block(ucBlk);
    if ((pstBlock == NULL) || (pstBlock->pucRam == NULL) ||
        (pstBlock->usBlockLen < (uint16_t)PARAM_CRC_BYTES_BLOCK))
    {
        return;
    }
    usCrcOff = (uint16_t)(pstBlock->usBlockLen - (uint16_t)PARAM_CRC_BYTES_BLOCK);
    pstBlock->pucRam[usCrcOff] ^= 0xFFu;
}

/**
 * @brief 测试用：头尾 magic 是否有效
 */
uint8_t DcTestParamSramOk(void)
{
    return param_sram_ok();
}
#endif /* DC_TEST */
