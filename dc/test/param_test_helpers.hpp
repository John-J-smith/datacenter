#pragma once

#include <gtest/gtest.h>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "datacenter.h"
#include "dc_param_attr.h"
#include "dc_test_param.h"
#include "dc_test_storage.h"
}

#define PARAM_TEST_NAME_CASE(name, ...) \
    case name:                        \
        return #name;

inline const char *ParamTypeName(uint16_t type)
{
    switch (type)
    {
        PARAM_ITEM_LIST(PARAM_TEST_NAME_CASE)
    default:
        return "?";
    }
}

#undef PARAM_TEST_NAME_CASE

inline const ST_PARAM_TABLE *ParamFindEntry(uint16_t type)
{
    for (uint16_t i = 0u; i < tParamApiTableCount; ++i)
    {
        if (tParamApiTable[i].eParamType == type)
        {
            return &tParamApiTable[i];
        }
    }
    return NULL;
}

inline void ParamCorruptBlockCrc(uint8_t blk)
{
    const ST_PARAM_BLOCK_TABLE *block;

    if ((uint16_t)blk >= tParamBlockTableCount)
    {
        return;
    }
    block = &tParamBlockTable[blk];
    if (block->ram == NULL)
    {
        return;
    }
    block->ram[block->ucBlockLen - PARAM_CRC_BYTES_BLOCK] ^= 0xFFu;
}

inline std::string ParamTraceLabel(uint16_t row, const ST_PARAM_TABLE *entry, uint8_t index)
{
    std::ostringstream oss;

    oss << "row=" << row << " " << ParamTypeName(entry->eParamType)
        << "(type=" << entry->eParamType << ")"
        << " blk=" << static_cast<unsigned>(entry->eBlockName)
        << " off=" << entry->uParamOffset
        << " ucParamLen=" << static_cast<unsigned>(entry->ucParamLen)
        << " index=" << static_cast<unsigned>(index)
        << " alias=0x" << std::hex << ParaAliasBuild(entry->eParamType, index);
    return oss.str();
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
    SCOPED_TRACE(ParamTraceLabel(row, entry, index));
}

inline ::testing::AssertionResult ExpectParamBuffersEqual(const uint8_t *expected,
                                                          const uint8_t *actual,
                                                          uint16_t nbytes,
                                                          uint16_t row,
                                                          const ST_PARAM_TABLE *entry,
                                                          uint8_t index)
{
    for (uint16_t b = 0u; b < nbytes; ++b)
    {
        if (expected[b] != actual[b])
        {
            return ::testing::AssertionFailure()
                   << ParamTraceLabel(row, entry, index) << " byte=" << b << " expected=0x"
                   << std::hex << static_cast<unsigned>(expected[b]) << " actual=0x"
                   << static_cast<unsigned>(actual[b]);
        }
    }
    return ::testing::AssertionSuccess();
}

class ParamTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        DcTestStorageReset();
        DcTestParamReset();
    }
};
