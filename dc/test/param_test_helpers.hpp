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

/// @brief 按小类枚举名返回字符串（PARAM_ITEM_LIST 展开）。
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

/// @brief 按 eParamType 查找 tParamApiTable 行。
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

/// @brief 翻转 SRAM 参数块末尾 CRC 的高字节（无 SRAM 的块忽略）。
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

/// @brief 拼 row/type/index/alias 的 SCOPED_TRACE 标签。
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

/// @brief LIST 用叶子 xy，其余类型用线性 ordinal。
inline uint8_t ParamIoIndex(const ST_PARAM_TABLE *entry, uint8_t ordinal)
{
    if (param_attr_type(entry) == (uint8_t)DATATYPE_LIST)
    {
        return param_attr_list_leaf_xy(entry->pAttr, ordinal);
    }
    return ordinal;
}

/// @brief 分项个数（LIST 为叶子数）。
inline uint8_t ParamIndexCount(const ST_PARAM_TABLE *entry)
{
    return param_attr_index_count(entry);
}

/// @brief 指定 index（含 xy / xF / 0xFF）的元素字节数。
inline uint16_t ParamElemBytes(const ST_PARAM_TABLE *entry, uint8_t index)
{
    return param_attr_elem_bytes(entry, index);
}

/// @brief 条目逻辑总字节。
inline uint16_t ParamTotalBytes(const ST_PARAM_TABLE *entry)
{
    return param_attr_total_bytes(entry);
}

/// @brief 映射表中单条参数最大逻辑字节（随 cfg 自动扩展）。
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

/// @brief 分配参数读写缓冲，长度为当前映射表单条上限。
inline std::vector<uint8_t> MakeParamIoBuffer(void)
{
    return std::vector<uint8_t>(MaxParamIoBytes(), 0u);
}

/// @brief 按 row/index 生成可识别的写入图案，便于读写回读比对。
inline void FillParamWritePattern(uint8_t *buf, uint16_t nbytes, uint16_t row, uint8_t index)
{
    for (uint16_t b = 0u; b < nbytes; ++b)
    {
        buf[b] = static_cast<uint8_t>(0x50u + static_cast<uint8_t>(row & 0x0Fu) + index +
                                      static_cast<uint8_t>(b & 0x0Fu));
    }
}

/// @brief 对本轮断言附加 ParamTraceLabel。
inline void TraceParamEntry(uint16_t row, const ST_PARAM_TABLE *entry, uint8_t index)
{
    SCOPED_TRACE(ParamTraceLabel(row, entry, index));
}

/// @brief 逐字节比较缓冲，失败时带参数行标签。
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
