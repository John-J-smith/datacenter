#pragma once

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "datacenter.h"
#include "dc_param_attr.h"
}

inline uint8_t ParamIndexCount(const ST_PARAM_TABLE *entry)
{
    return param_attr_index_count(entry);
}

inline uint16_t ParamElemBytes(const ST_PARAM_TABLE *entry, uint8_t index)
{
    return param_attr_elem_bytes(entry, index);
}

inline uint16_t ParamTotalBytes(const ST_PARAM_TABLE *entry)
{
    return param_attr_total_bytes(entry);
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

inline void TraceParamEntry(uint16_t row, const ST_PARAM_TABLE *entry, uint8_t index)
{
    SCOPED_TRACE("row=" + std::to_string(row) + " type=" +
                 std::to_string(entry->eParamType) + " index=" +
                 std::to_string(static_cast<unsigned>(index)));
}

class ParamTestBase : public ::testing::Test {};
