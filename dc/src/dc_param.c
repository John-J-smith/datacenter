#include "dc_entry.h"
#include "datacenter.h"
#include "dc_param.h"
#include "dc_param_attr.h"

#define DC_PARAM_LAYOUT_DEFINE

#include "dc_param_layout.h"

#include <string.h>

static uint8_t s_param_inited;

static const ST_PARAM_BLOCK_TABLE *param_find_block(uint8_t blk)
{
    if ((uint16_t)blk >= tParamBlockTableCount)
    {
        return 0;
    }
    return &tParamBlockTable[blk];
}

static void param_ensure_init(void)
{
    uint16_t i;

    if (s_param_inited != 0u)
    {
        return;
    }
    for (i = 0u; i < tParamBlockTableCount; i++)
    {
        uint16_t payload;

        payload = (uint16_t)(tParamBlockTable[i].ucBlockLen - (uint16_t)PARAM_CRC_BYTES_BLOCK);
        memset(tParamBlockTable[i].ram, 0xFF, payload);
    }
    for (i = 0u; i < tParamApiTableCount; i++)
    {
        const ST_PARAM_TABLE *item;
        const ST_PARAM_BLOCK_TABLE *block;

        item = &tParamApiTable[i];
        if (item->pDefault == NULL)
        {
            continue;
        }
        block = param_find_block(item->eBlockName);
        if ((block == 0) || (block->ram == 0))
        {
            continue;
        }
        memcpy(block->ram + item->uParamOffset, item->pDefault, item->ucParamLen);
    }
    s_param_inited = 1u;
}

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
    return 0;
}

static int16_t param_xfer_link(const ST_PARAM_TABLE *item, uint8_t *rw,
                               const uint8_t *ro, uint16_t usLen, uint8_t index,
                               int writing)
{
    const uint8_t *attr;
    uint8_t k;
    uint8_t per_page;
    uint16_t i;
    uint16_t copied;

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
        if ((block == 0) || (block->ram == 0))
        {
            return DC_RET_ALIAS_ERR;
        }
        off = (uint16_t)slot * (uint16_t)k;
        if (writing != 0)
        {
            memcpy(block->ram + off, ro + copied, k);
        }
        else
        {
            memcpy(rw + copied, block->ram + off, k);
        }
        copied = (uint16_t)(copied + k);
    }
    return (int16_t)copied;
}

static int16_t param_xfer_struct(const ST_PARAM_TABLE *item,
                                 const ST_PARAM_BLOCK_TABLE *block,
                                 uint8_t *rw, const uint8_t *ro,
                                 uint16_t usLen, uint8_t index, int writing)
{
    uint16_t copied;
    uint16_t i;
    uint8_t idx;

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
        if (writing != 0)
        {
            memcpy(block->ram + off, ro + copied, eb);
        }
        else
        {
            memcpy(rw + copied, block->ram + off, eb);
        }
        copied = (uint16_t)(copied + eb);
        idx = (uint8_t)(idx + 1u);
    }
    return (int16_t)copied;
}

static int16_t param_xfer(uint32_t alias, uint8_t *rw, const uint8_t *ro,
                          uint16_t usLen, uint8_t type, int writing)
{
    const ST_PARAM_TABLE *item;
    const ST_PARAM_BLOCK_TABLE *block;
    uint8_t index;
    uint8_t dtype;
    uint8_t index_max;
    uint16_t nbytes;
    uint16_t off;
    uint8_t elem_bytes;

    param_ensure_init();

    if (usLen == 0u)
    {
        return 0;
    }

    item = param_find_item(ParaAliasToType(alias));
    if (item == 0)
    {
        return DC_RET_ALIAS_ERR;
    }

    block = param_find_block(item->eBlockName);
    if ((block == 0) || (block->ram == 0))
    {
        return DC_RET_ALIAS_ERR;
    }

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
        memcpy(block->ram + off, ro, nbytes);
    }
    else
    {
        memcpy(rw, block->ram + off, nbytes);
    }
    return (int16_t)nbytes;
}

int16_t dc_read_param(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return param_xfer(alias, dataPtr, 0, usLen, type, 0);
}

int16_t dc_write_param(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return param_xfer(alias, 0, dataPtr, usLen, type, 1);
}

#ifdef DC_TEST
void DcTestParamReset(void)
{
    uint16_t i;

    s_param_inited = 0u;
    for (i = 0u; i < tParamBlockTableCount; i++)
    {
        if (tParamBlockTable[i].ram != 0)
        {
            memset(tParamBlockTable[i].ram, 0, (size_t)tParamBlockTable[i].ucBlockLen);
        }
    }
}
#endif /* DC_TEST */
