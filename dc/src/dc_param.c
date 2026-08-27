#include "dc_entry.h"
#include "datacenter.h"
#include "dc_param.h"

#define DC_PARAM_LAYOUT_DEFINE

#include "dc_param_layout.h"

#include <string.h>

static uint8_t s_param_inited;

static void param_ensure_init(void)
{
    uint16_t i;

    if (s_param_inited != 0u)
    {
        return;
    }
    for (i = 0u; i < tParamBlockTableCount; i++)
    {
        memcpy(tParamBlockTable[i].ram, tParamBlockTable[i].ucPtr, tParamBlockTable[i].ucBlockLen);
    }
    s_param_inited = 1u;
}

static const STR_PARAMETER_TABLE *param_find_item(uint16_t subclass)
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

static const STR_PARAM_BLOCK_TABLE *param_find_block(uint8_t name)
{
    uint16_t i;

    for (i = 0u; i < tParamBlockTableCount; i++)
    {
        if (tParamBlockTable[i].eBlockName == name)
        {
            return &tParamBlockTable[i];
        }
    }
    return 0;
}

static int16_t param_xfer_link(const STR_PARAMETER_TABLE *item, uint8_t *rw,
                               const uint8_t *ro, uint16_t usLen, uint8_t index,
                               int writing)
{
    uint8_t k;
    uint8_t per_page;
    uint16_t i;
    uint16_t copied;

    k = item->ucBytes;
    if (k == 0u)
    {
        return DC_RET_PARAM_ERR;
    }
    per_page = (uint8_t)(PARAM_BLOCK_PAYLOAD_MAX / (uint16_t)k);
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
        const STR_PARAM_BLOCK_TABLE *block;
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

static int16_t param_xfer(uint32_t genre, uint8_t *rw, const uint8_t *ro,
                          uint16_t usLen, uint8_t type, int writing)
{
    const STR_PARAMETER_TABLE *item;
    const STR_PARAM_BLOCK_TABLE *block;
    uint8_t index;
    uint16_t nbytes;
    uint16_t off;

    param_ensure_init();

    if (usLen == 0u)
    {
        return 0;
    }

    item = param_find_item(ParaAliasToType(genre));
    if (item == 0)
    {
        return DC_RET_ALIAS_ERR;
    }

    block = param_find_block(item->eBlockName);
    if ((block == 0) || (block->ram == 0))
    {
        return DC_RET_ALIAS_ERR;
    }

    index = GetAliasIndex(genre);
    if (index == PARAM_INDEX_ALL)
    {
        index = 0u;
        usLen = item->ucIndexNum;
    }

    if ((item->ucDataType == (uint8_t)DATATYPE_LIST) && (usLen > 1u))
    {
        return DC_RET_PARAM_ERR;
    }

    if ((uint16_t)index + usLen > (uint16_t)item->ucIndexNum)
    {
        return DC_RET_PARAM_ERR;
    }

    if (item->ucDataType == (uint8_t)DATATYPE_LINKARRAY)
    {
        return param_xfer_link(item, rw, ro, usLen, index, writing);
    }

    nbytes = (uint16_t)(usLen * (uint16_t)item->ucBytes);
    off = (uint16_t)(item->uParamOffset + (uint16_t)index * (uint16_t)item->ucBytes);

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

int16_t ReadParamData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return param_xfer(genre, dataPtr, 0, usLen, type, 0);
}

int16_t WriteParamData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    return param_xfer(genre, 0, dataPtr, usLen, type, 1);
}
