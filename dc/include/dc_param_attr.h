#ifndef DC_PARAM_ATTR_H
#define DC_PARAM_ATTR_H

#include <stdint.h>
#include "dc_param.h"

static inline uint8_t param_attr_bytes_type(const uint8_t *attr)
{
    if (attr == 0)
    {
        return 0xFFu;
    }
    return attr[0];
}

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

static inline uint8_t param_attr_bytes_index_count(const uint8_t *attr, uint8_t param_len,
                                                   uint16_t total_len)
{
    uint8_t npage;
    uint8_t per_page;
    uint16_t nrec;

    if (attr == 0)
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
        return (uint8_t)((uint16_t)attr[1] * (uint16_t)attr[2]);
    default:
        return 0u;
    }
    (void)param_len;
}

static inline uint8_t param_attr_bytes_elem_bytes(const uint8_t *attr, uint8_t param_len,
                                                  uint8_t index)
{
    if (attr == 0)
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

static inline uint16_t param_attr_bytes_struct_field_off(const uint8_t *attr, uint8_t index)
{
    uint8_t i;
    uint16_t off;

    if (attr == 0)
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

static inline uint16_t param_attr_bytes_total_bytes(const uint8_t *attr, uint8_t param_len,
                                                    uint16_t total_len)
{
    uint16_t sum;
    uint8_t i;

    if (attr == 0)
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
        return (uint16_t)((uint16_t)attr[1] * (uint16_t)attr[2] * (uint16_t)attr[3]);
    default:
        return 0u;
    }
}

static inline uint8_t param_attr_type(const ST_PARAM_TABLE *item)
{
    if ((item == 0) || (item->pAttr == 0))
    {
        return 0xFFu;
    }
    return param_attr_bytes_type(item->pAttr);
}

static inline uint8_t param_attr_index_count(const ST_PARAM_TABLE *item)
{
    if (item == 0)
    {
        return 0u;
    }
    return param_attr_bytes_index_count(item->pAttr, item->ucParamLen, item->ucParamLen);
}

static inline uint8_t param_attr_elem_bytes(const ST_PARAM_TABLE *item, uint8_t index)
{
    if (item == 0)
    {
        return 0u;
    }
    return param_attr_bytes_elem_bytes(item->pAttr, item->ucParamLen, index);
}

static inline uint16_t param_attr_struct_field_off(const ST_PARAM_TABLE *item, uint8_t index)
{
    if (item == 0)
    {
        return 0u;
    }
    return param_attr_bytes_struct_field_off(item->pAttr, index);
}

static inline uint16_t param_attr_total_bytes(const ST_PARAM_TABLE *item)
{
    if (item == 0)
    {
        return 0u;
    }
    return param_attr_bytes_total_bytes(item->pAttr, item->ucParamLen, item->ucParamLen);
}

#endif /* DC_PARAM_ATTR_H */
