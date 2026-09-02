#pragma once

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "datacenter.h"
}

inline uint8_t ParamAttrType(const ST_PARAM_TABLE *entry)
{
    return entry->ucPtr[0];
}

inline uint8_t ParamIndexCount(const ST_PARAM_TABLE *entry)
{
    const uint8_t *attr = entry->ucPtr;

    switch (static_cast<E_PARAM_STORAGE_DATATYPE>(attr[0]))
    {
    case DATATYPE_INT:
        return 1u;
    case DATATYPE_ARRAY:
    case DATATYPE_STRUCT:
        return attr[1];
    case DATATYPE_LINKARRAY:
        return static_cast<uint8_t>(attr[1] * attr[2]);
    default:
        return 0u;
    }
}

inline uint16_t ParamElemBytes(const ST_PARAM_TABLE *entry, uint8_t index)
{
    const uint8_t *attr = entry->ucPtr;

    switch (static_cast<E_PARAM_STORAGE_DATATYPE>(attr[0]))
    {
    case DATATYPE_INT:
        return entry->ucParamLen;
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

inline uint16_t ParamTotalBytes(const ST_PARAM_TABLE *entry)
{
    const uint8_t *attr = entry->ucPtr;
    uint16_t sum;
    uint8_t i;

    switch (static_cast<E_PARAM_STORAGE_DATATYPE>(attr[0]))
    {
    case DATATYPE_INT:
        return entry->ucParamLen;
    case DATATYPE_ARRAY:
        return static_cast<uint16_t>(attr[1]) * static_cast<uint16_t>(attr[2]);
    case DATATYPE_STRUCT:
        sum = 0u;
        for (i = 0u; i < attr[1]; ++i)
        {
            sum = static_cast<uint16_t>(sum + attr[2u + i]);
        }
        return sum;
    case DATATYPE_LINKARRAY:
        return static_cast<uint16_t>(attr[1]) * static_cast<uint16_t>(attr[2]) *
               static_cast<uint16_t>(attr[3]);
    default:
        return 0u;
    }
}

inline uint16_t MaxParamIoBytes(void)
{
    uint16_t max_bytes = 1u;

    for (uint16_t i = 0u; i < tParamApiTableCount; ++i)
    {
        const uint16_t total = ParamTotalBytes(&tParamApiTable[i]);
        if (total > max_bytes)
        {
            max_bytes = total;
        }
    }
    return max_bytes;
}

inline std::vector<uint8_t> MakeParamIoBuffer(void)
{
    return std::vector<uint8_t>(MaxParamIoBytes(), 0u);
}

inline void FillParamWritePattern(uint8_t *buf, uint16_t nbytes, uint16_t row, uint8_t index)
{
    for (uint16_t b = 0u; b < nbytes; ++b)
    {
        buf[b] = static_cast<uint8_t>(0x50u + static_cast<uint8_t>(row & 0x0Fu) + index +
                                      static_cast<uint8_t>(b & 0x0Fu));
    }
}

inline uint32_t ParamAlias(E_PARAMETER_TYPE type, uint8_t index)
{
    switch (type)
    {
    case PARAM_REMOTECTRL:
        return DC_ALIAS_PARAM_REMOTECTRL_0;
    case PARAM_SEASON_SWTIME:
        return DC_ALIAS_PARAM_SEASON_SWTIME;
    case PARAM_DAY_SWTIME:
        return DC_ALIAS_PARAM_DAY_SWTIME;
    case PARAM_FEE_SWTIME:
        return DC_ALIAS_PARAM_FEE_SWTIME;
    case PARAM_LADDER_SWTIME:
        return DC_ALIAS_PARAM_LADDER_SWTIME;
    case PARAM_HOLIDAY_DATA:
        if (index == PARAM_INDEX_ALL)
        {
            return DC_ALIAS_PARAM_HOLIDAY_DATA_ALL;
        }
        return static_cast<uint32_t>(DC_ALIAS_PARAM_HOLIDAY_DATA_0) +
               static_cast<uint32_t>(index);
    case PARAM_CALIB_DATA:
        if (index == PARAM_INDEX_ALL)
        {
            return DC_ALIAS_PARAM_CALIB_DATA_ALL;
        }
        return static_cast<uint32_t>(DC_ALIAS_PARAM_CALIB_DATA_0) +
               static_cast<uint32_t>(index);
    case PARAM_UN:
        return DC_ALIAS_PARAM_UN;
    case PARAM_IB:
        return DC_ALIAS_PARAM_IB;
    case PARAM_IMAX:
        return DC_ALIAS_PARAM_IMAX;
    default:
        return 0u;
    }
}

inline void TraceParamEntry(uint16_t row, const ST_PARAM_TABLE *entry, uint8_t index)
{
    SCOPED_TRACE("row=" + std::to_string(row) + " type=" +
                 std::to_string(entry->eParamType) + " index=" +
                 std::to_string(static_cast<unsigned>(index)));
}

class ParamTestBase : public ::testing::Test {};
