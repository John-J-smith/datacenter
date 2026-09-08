/**
 * @file dc_param_attr.h
 * @brief 参变量分项 attrib 表解析（pack 生成，runtime / 测试共用）
 *
 * attrib 字节布局（pucAttr[0] 为 DATATYPE_*）：
 *   INT       : [type]
 *   ARRAY     : [type, elem_count, elem_bytes]
 *   STRUCT    : [type, field_count, field0_len, field1_len, ...]
 *   LIST      : [type, leaf_count, xy0, len0, xy1, len1, ...]
 *               仅叶子；xF / 0xFF 由查找推导，不写进表
 *   LINKARRAY : [type, N, M, K]
 *               已解析：N=子块数，M=每块容量（条），K=每条字节数；记录数 = total_len/K
 *               未解析（N=M=0）：由 total_len 与 PARAM_BLOCK_PAYLOAD_MAX 推导
 */
#ifndef DC_PARAM_ATTR_H
#define DC_PARAM_ATTR_H

#include <stddef.h>
#include <stdint.h>
#include "dc_param.h"

/**
 * @brief 读取 attrib 表存储类型字节
 *
 * @param pucAttr attrib 表指针
 * @return DATATYPE_*；pucAttr 为 NULL 时返回 0xFF
 */
static inline uint8_t param_attr_bytes_type(const uint8_t *pucAttr)
{
    if (pucAttr == NULL)
    {
        return 0xFFu;
    }
    return pucAttr[0];
}

/**
 * @brief 推导 LINKARRAY 分页维度（N / M / 记录总数）
 *
 * @param total_len  逻辑总字节数
 * @param k          每条记录字节数
 * @param payload_max 单块 payload 上限
 * @param npage      输出：子块数 N
 * @param per_page   输出：每块条数 M
 * @param nrec       输出：记录总数
 * @return 非 0 表示推导成功
 */
static inline uint8_t param_linkarray_dims(uint16_t total_len, uint8_t k, uint16_t payload_max,
                                       uint8_t *npage, uint8_t *per_page, uint16_t *nrec)
{
    uint16_t rec;
    uint32_t ulPp;
    uint32_t ulNp;

    if ((k == 0u) || (payload_max == 0u) || (total_len < k))
    {
        return 0;
    }
    if ((total_len % k) != 0u)
    {
        return 0;
    }
    rec = (uint16_t)(total_len / k);
    ulPp = (uint32_t)payload_max / (uint32_t)k;
    if (ulPp == 0u)
    {
        return 0;
    }
    ulNp = ((uint32_t)rec + ulPp - 1u) / ulPp;
    if (ulNp > 255u)
    {
        return 0;
    }
    *nrec = rec;
    *per_page = (uint8_t)ulPp;
    *npage = (uint8_t)ulNp;
    return 1;
}

/**
 * @brief 从 attrib 字节表求分项 index 个数
 *
 * @param pucAttr      attrib 表指针
 * @param param_len API 表 ucParamLen（INT 元素长度等）
 * @param total_len 逻辑总字节（LINKARRAY 未解析 N/M 时使用）
 * @return index 个数；无法解析时返回 0
 */
static inline uint8_t param_attr_bytes_index_count(const uint8_t *pucAttr, 
                                                   uint8_t param_len,
                                                   uint16_t total_len)
{
    uint8_t npage;
    uint8_t per_page;
    uint16_t nrec;

    if (pucAttr == NULL)
    {
        return 0u;
    }
    switch ((E_PARAM_STORAGE_DATATYPE)pucAttr[0])
    {
    case DATATYPE_INT:
        return 1u;
    case DATATYPE_ARRAY:
    case DATATYPE_STRUCT:
        return pucAttr[1];
    case DATATYPE_LIST:
        return pucAttr[1];
    case DATATYPE_LINKARRAY:
        if ((pucAttr[1] == 0u) && (pucAttr[2] == 0u))
        {
            if (param_linkarray_dims(total_len, pucAttr[3], PARAM_BLOCK_PAYLOAD_MAX,
                                   &npage, &per_page, &nrec) == 0)
            {
                return 0u;
            }
            return (uint8_t)nrec;
        }
        if ((pucAttr[3] != 0u) && (total_len >= (uint16_t)pucAttr[3]) &&
            ((total_len % (uint16_t)pucAttr[3]) == 0u))
        {
            return (uint8_t)(total_len / (uint16_t)pucAttr[3]);
        }
        return (uint8_t)((uint16_t)pucAttr[1] * (uint16_t)pucAttr[2]);
    default:
        return 0u;
    }
    (void)param_len;
}

/**
 * @brief LIST：按分项号求相对条目起点的偏移与长度
 *
 * @param pucAttr  attrib 表
 * @param index 叶子 xy、组全部 xF，或 PARAM_INDEX_ALL
 * @param off   输出偏移
 * @param len   输出字节数
 * @return 非 0 表示命中
 */
static inline uint8_t param_attr_list_lookup(const uint8_t *pucAttr, uint8_t index,
                                         uint16_t *off, uint16_t *len)
{
    uint8_t n;
    uint8_t xy;
    uint8_t glen_g;
    uint16_t run;
    uint16_t sum;
    uint16_t group_off;
    uint16_t group_len;
    uint8_t ucFound;

    if ((pucAttr == NULL) || (off == NULL) || (len == NULL) ||
        (pucAttr[0] != (uint8_t)DATATYPE_LIST))
    {
        return 0;
    }
    n = pucAttr[1];
    run = 0u;
    sum = 0u;
    for (uint8_t i = 0u; i < n; i++)
    {
        sum = (uint16_t)(sum + (uint16_t)pucAttr[3u + (uint8_t)(i * 2u)]);
    }
    if (index == PARAM_INDEX_ALL)
    {
        *off = 0u;
        *len = sum;
        return (sum != 0u) ? 1 : 0;
    }
    if ((index & 0x0Fu) == 0x0Fu)
    {
        glen_g = (uint8_t)(index >> 4);
        ucFound = 0u;
        group_off = 0u;
        group_len = 0u;
        run = 0u;
        for (uint8_t i = 0u; i < n; i++)
        {
            xy = pucAttr[2u + (uint8_t)(i * 2u)];
            if ((uint8_t)(xy >> 4) == glen_g)
            {
                if (ucFound == 0u)
                {
                    group_off = run;
                    ucFound = 1u;
                }
                group_len = (uint16_t)(group_len + (uint16_t)pucAttr[3u + (uint8_t)(i * 2u)]);
            }
            else if (ucFound != 0u)
            {
                break;
            }
            run = (uint16_t)(run + (uint16_t)pucAttr[3u + (uint8_t)(i * 2u)]);
        }
        if ((ucFound == 0u) || (group_len == 0u))
        {
            return 0;
        }
        *off = group_off;
        *len = group_len;
        return 1;
    }
    run = 0u;
    for (uint8_t i = 0u; i < n; i++)
    {
        xy = pucAttr[2u + (uint8_t)(i * 2u)];
        if (xy == index)
        {
            *off = run;
            *len = (uint16_t)pucAttr[3u + (uint8_t)(i * 2u)];
            return (*len != 0u) ? 1 : 0;
        }
        run = (uint16_t)(run + (uint16_t)pucAttr[3u + (uint8_t)(i * 2u)]);
    }
    return 0;
}

/**
 * @brief LIST：第 ordinal 个叶子的分项号 xy
 *
 * @param pucAttr     attrib 表
 * @param ordinal  叶子序号（0 起）
 * @return xy；越界返回 0
 */
static inline uint8_t param_attr_list_leaf_xy(const uint8_t *pucAttr, uint8_t ordinal)
{
    if ((pucAttr == NULL) || (pucAttr[0] != (uint8_t)DATATYPE_LIST) || (ordinal >= pucAttr[1]))
    {
        return 0u;
    }
    return pucAttr[2u + (uint8_t)(ordinal * 2u)];
}

/**
 * @brief 从 attrib 字节表求指定 index 的元素字节数
 *
 * @param pucAttr      attrib 表指针
 * @param param_len API 表 ucParamLen（INT 时使用）
 * @param index     分项索引
 * @return 元素字节数；越界或无效时返回 0
 */
static inline uint8_t param_attr_bytes_elem_bytes(const uint8_t *pucAttr, 
                                                  uint8_t param_len,
                                                  uint8_t index)
{
    uint16_t off;
    uint16_t len;

    if (pucAttr == NULL)
    {
        return 0u;
    }
    switch ((E_PARAM_STORAGE_DATATYPE)pucAttr[0])
    {
    case DATATYPE_INT:
        return param_len;
    case DATATYPE_ARRAY:
        return pucAttr[2];
    case DATATYPE_STRUCT:
        if (index >= pucAttr[1])
        {
            return 0u;
        }
        return pucAttr[2u + index];
    case DATATYPE_LIST:
        if (param_attr_list_lookup(pucAttr, index, &off, &len) == 0)
        {
            return 0u;
        }
        if (len > 255u)
        {
            return 0u;
        }
        return (uint8_t)len;
    case DATATYPE_LINKARRAY:
        return pucAttr[3];
    default:
        return 0u;
    }
}

/**
 * @brief STRUCT 类型：求字段 index 在结构体内的字节偏移
 *
 * @param pucAttr  attrib 表指针
 * @param index 字段索引（从 0 起）
 * @return 相对结构体起始的偏移；无效时返回 0
 */
static inline uint16_t param_attr_bytes_struct_field_off(const uint8_t *pucAttr, uint8_t index)
{
    uint16_t off;

    if (pucAttr == NULL)
    {
        return 0u;
    }
    off = 0u;
    for (uint8_t i = 0u; i < index; i++)
    {
        off = (uint16_t)(off + (uint16_t)pucAttr[2u + i]);
    }
    return off;
}

/**
 * @brief 从 attrib 字节表求逻辑总字节数
 *
 * @param pucAttr      attrib 表指针
 * @param param_len API 表 ucParamLen
 * @param total_len LINKARRAY 未解析时的 catalog 总长度
 * @return 总字节数；无效时返回 0
 */
static inline uint16_t param_attr_bytes_total_bytes(const uint8_t *pucAttr, 
                                                    uint8_t param_len,
                                                    uint16_t total_len)
{
    uint16_t sum;

    if (pucAttr == NULL)
    {
        return 0u;
    }
    switch ((E_PARAM_STORAGE_DATATYPE)pucAttr[0])
    {
    case DATATYPE_INT:
        return param_len;
    case DATATYPE_ARRAY:
        return (uint16_t)((uint16_t)pucAttr[1] * (uint16_t)pucAttr[2]);
    case DATATYPE_STRUCT:
        sum = 0u;
        for (uint8_t i = 0u; i < pucAttr[1]; i++)
        {
            sum = (uint16_t)(sum + pucAttr[2u + i]);
        }
        return sum;
    case DATATYPE_LIST:
        sum = 0u;
        for (uint8_t i = 0u; i < pucAttr[1]; i++)
        {
            sum = (uint16_t)(sum + pucAttr[3u + (uint8_t)(i * 2u)]);
        }
        return sum;
    case DATATYPE_LINKARRAY:
        if ((pucAttr[1] == 0u) && (pucAttr[2] == 0u))
        {
            return total_len;
        }
        return total_len;
    default:
        return 0u;
    }
}

/**
 * @brief 从 API 表项读取存储类型
 *
 * @param pstItem ST_PARAM_TABLE 指针
 * @return DATATYPE_*；无效时返回 0xFF
 */
static inline uint8_t param_attr_type(const ST_PARAM_TABLE *pstItem)
{
    if ((pstItem == NULL) || (pstItem->pucAttr == NULL))
    {
        return 0xFFu;
    }
    return param_attr_bytes_type(pstItem->pucAttr);
}

/**
 * @brief 从 API 表项求分项 index 个数
 *
 * @param pstItem ST_PARAM_TABLE 指针
 * @return index 个数
 */
static inline uint8_t param_attr_index_count(const ST_PARAM_TABLE *pstItem)
{
    if (pstItem == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_index_count(pstItem->pucAttr, pstItem->ucParamLen, pstItem->ucParamLen);
}

/**
 * @brief 从 API 表项求指定 index 的元素字节数
 *
 * @param pstItem  ST_PARAM_TABLE 指针
 * @param index 分项索引
 * @return 元素字节数
 */
static inline uint8_t param_attr_elem_bytes(const ST_PARAM_TABLE *pstItem, uint8_t index)
{
    if (pstItem == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_elem_bytes(pstItem->pucAttr, pstItem->ucParamLen, index);
}

/**
 * @brief STRUCT 类型：从 API 表项求字段 index 的字节偏移
 *
 * @param pstItem  ST_PARAM_TABLE 指针
 * @param index 字段索引
 * @return 相对 ucParamOffset 的字段偏移
 */
static inline uint16_t param_attr_struct_field_off(const ST_PARAM_TABLE *pstItem, uint8_t index)
{
    if (pstItem == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_struct_field_off(pstItem->pucAttr, index);
}

/**
 * @brief 从 API 表项求逻辑总字节数
 *
 * @param pstItem ST_PARAM_TABLE 指针
 * @return 总字节数
 */
static inline uint16_t param_attr_total_bytes(const ST_PARAM_TABLE *pstItem)
{
    if (pstItem == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_total_bytes(pstItem->pucAttr, pstItem->ucParamLen, pstItem->ucParamLen);
}

#endif /* DC_PARAM_ATTR_H */
