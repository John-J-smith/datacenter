/**
 * @file dc_param_attr.h
 * @brief 参变量分项 attrib 表解析（pack 生成，runtime / 测试共用）
 *
 * attrib 字节布局（attr[0] 为 DATATYPE_*）：
 *   INT       : [type]
 *   ARRAY     : [type, index_count, elem_bytes]
 *   STRUCT    : [type, field_count, field0_len, field1_len, ...]
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
 * @param attr attrib 表指针
 * @return DATATYPE_*；attr 为 NULL 时返回 0xFF
 */
static inline uint8_t param_attr_bytes_type(const uint8_t *attr)
{
    if (attr == NULL)
    {
        return 0xFFu;
    }
    return attr[0];
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
static inline int param_linkarray_dims(uint16_t total_len, uint8_t k, uint16_t payload_max,
                                       uint8_t *npage, uint8_t *per_page, uint16_t *nrec)
{
    uint16_t rec;
    unsigned pp;
    unsigned np;

    if ((k == 0u) || (payload_max == 0u) || (total_len < k))
    {
        return 0;
    }
    if ((total_len % k) != 0u)
    {
        return 0;
    }
    rec = (uint16_t)(total_len / k);
    pp = (unsigned)payload_max / (unsigned)k;
    if (pp == 0u)
    {
        return 0;
    }
    np = ((unsigned)rec + pp - 1u) / pp;
    if (np > 255u)
    {
        return 0;
    }
    *nrec = rec;
    *per_page = (uint8_t)pp;
    *npage = (uint8_t)np;
    return 1;
}

/**
 * @brief 从 attrib 字节表求分项 index 个数
 *
 * @param attr      attrib 表指针
 * @param param_len API 表 ucParamLen（INT 元素长度等）
 * @param total_len 逻辑总字节（LINKARRAY 未解析 N/M 时使用）
 * @return index 个数；无法解析时返回 0
 */
static inline uint8_t param_attr_bytes_index_count(const uint8_t *attr, 
                                                   uint8_t param_len,
                                                   uint16_t total_len)
{
    uint8_t npage;
    uint8_t per_page;
    uint16_t nrec;

    if (attr == NULL)
    {
        return 0u;
    }
    switch ((E_PARAM_STORAGE_DATATYPE)attr[0])
    {
    case DATATYPE_INT:
        return 1u;
    case DATATYPE_ARRAY:
    case DATATYPE_STRUCT:
        return attr[1];
    case DATATYPE_LINKARRAY:
        if ((attr[1] == 0u) && (attr[2] == 0u))
        {
            if (param_linkarray_dims(total_len, attr[3], PARAM_BLOCK_PAYLOAD_MAX,
                                   &npage, &per_page, &nrec) == 0)
            {
                return 0u;
            }
            return (uint8_t)nrec;
        }
        if ((attr[3] != 0u) && (total_len >= (uint16_t)attr[3]) &&
            ((total_len % (uint16_t)attr[3]) == 0u))
        {
            return (uint8_t)(total_len / (uint16_t)attr[3]);
        }
        return (uint8_t)((uint16_t)attr[1] * (uint16_t)attr[2]);
    default:
        return 0u;
    }
    (void)param_len;
}

/**
 * @brief 从 attrib 字节表求指定 index 的元素字节数
 *
 * @param attr      attrib 表指针
 * @param param_len API 表 ucParamLen（INT 时使用）
 * @param index     分项索引
 * @return 元素字节数；越界或无效时返回 0
 */
static inline uint8_t param_attr_bytes_elem_bytes(const uint8_t *attr, 
                                                  uint8_t param_len,
                                                  uint8_t index)
{
    if (attr == NULL)
    {
        return 0u;
    }
    switch ((E_PARAM_STORAGE_DATATYPE)attr[0])
    {
    case DATATYPE_INT:
        return param_len;
    case DATATYPE_ARRAY:
        return attr[2];
    case DATATYPE_STRUCT:
        if (index >= attr[1])
        {
            return 0u;
        }
        return attr[2u + index];
    case DATATYPE_LINKARRAY:
        return attr[3];
    default:
        return 0u;
    }
}

/**
 * @brief STRUCT 类型：求字段 index 在结构体内的字节偏移
 *
 * @param attr  attrib 表指针
 * @param index 字段索引（从 0 起）
 * @return 相对结构体起始的偏移；无效时返回 0
 */
static inline uint16_t param_attr_bytes_struct_field_off(const uint8_t *attr, uint8_t index)
{
    uint8_t i;
    uint16_t off;

    if (attr == NULL)
    {
        return 0u;
    }
    off = 0u;
    for (i = 0u; i < index; i++)
    {
        off = (uint16_t)(off + (uint16_t)attr[2u + i]);
    }
    return off;
}

/**
 * @brief 从 attrib 字节表求逻辑总字节数
 *
 * @param attr      attrib 表指针
 * @param param_len API 表 ucParamLen
 * @param total_len LINKARRAY 未解析时的 catalog 总长度
 * @return 总字节数；无效时返回 0
 */
static inline uint16_t param_attr_bytes_total_bytes(const uint8_t *attr, 
                                                    uint8_t param_len,
                                                    uint16_t total_len)
{
    uint16_t sum;
    uint8_t i;

    if (attr == NULL)
    {
        return 0u;
    }
    switch ((E_PARAM_STORAGE_DATATYPE)attr[0])
    {
    case DATATYPE_INT:
        return param_len;
    case DATATYPE_ARRAY:
        return (uint16_t)((uint16_t)attr[1] * (uint16_t)attr[2]);
    case DATATYPE_STRUCT:
        sum = 0u;
        for (i = 0u; i < attr[1]; i++)
        {
            sum = (uint16_t)(sum + attr[2u + i]);
        }
        return sum;
    case DATATYPE_LINKARRAY:
        if ((attr[1] == 0u) && (attr[2] == 0u))
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
 * @param item ST_PARAM_TABLE 指针
 * @return DATATYPE_*；无效时返回 0xFF
 */
static inline uint8_t param_attr_type(const ST_PARAM_TABLE *item)
{
    if ((item == NULL) || (item->pAttr == NULL))
    {
        return 0xFFu;
    }
    return param_attr_bytes_type(item->pAttr);
}

/**
 * @brief 从 API 表项求分项 index 个数
 *
 * @param item ST_PARAM_TABLE 指针
 * @return index 个数
 */
static inline uint8_t param_attr_index_count(const ST_PARAM_TABLE *item)
{
    if (item == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_index_count(item->pAttr, item->ucParamLen, item->ucParamLen);
}

/**
 * @brief 从 API 表项求指定 index 的元素字节数
 *
 * @param item  ST_PARAM_TABLE 指针
 * @param index 分项索引
 * @return 元素字节数
 */
static inline uint8_t param_attr_elem_bytes(const ST_PARAM_TABLE *item, uint8_t index)
{
    if (item == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_elem_bytes(item->pAttr, item->ucParamLen, index);
}

/**
 * @brief STRUCT 类型：从 API 表项求字段 index 的字节偏移
 *
 * @param item  ST_PARAM_TABLE 指针
 * @param index 字段索引
 * @return 相对 uParamOffset 的字段偏移
 */
static inline uint16_t param_attr_struct_field_off(const ST_PARAM_TABLE *item, uint8_t index)
{
    if (item == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_struct_field_off(item->pAttr, index);
}

/**
 * @brief 从 API 表项求逻辑总字节数
 *
 * @param item ST_PARAM_TABLE 指针
 * @return 总字节数
 */
static inline uint16_t param_attr_total_bytes(const ST_PARAM_TABLE *item)
{
    if (item == NULL)
    {
        return 0u;
    }
    return param_attr_bytes_total_bytes(item->pAttr, item->ucParamLen, item->ucParamLen);
}

#endif /* DC_PARAM_ATTR_H */
