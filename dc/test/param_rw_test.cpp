#include "param_test_helpers.hpp"

#include <algorithm>

namespace {

// 测试内容：tParamApiTable 全部参变量、全部分项 index 读写回读（经别名，含 LIST xy）
TEST_F(ParamTestBase, AllParams_ReadWrite)
{
    std::vector<uint8_t> wbuf(MakeParamIoBuffer());
    std::vector<uint8_t> rbuf(MakeParamIoBuffer());

    // 1. 遍历映射表，逐项、逐分项写入图案
    for (uint16_t row = 0u; row < tParamApiTableCount; ++row)
    {
        const ST_PARAM_TABLE *entry = &tParamApiTable[row];
        const uint8_t index_count = ParamIndexCount(entry);

        for (uint8_t ordinal = 0u; ordinal < index_count; ++ordinal)
        {
            const uint8_t index = ParamIoIndex(entry, ordinal);
            const uint16_t elem_bytes = ParamElemBytes(entry, index);
            const uint32_t alias = ParaAliasBuild(entry->usParamType, index);
            
            TraceParamEntry(row, entry, index);
            FillParamWritePattern(wbuf.data(), elem_bytes, row, index);
            ASSERT_EQ(dc_write_alias(alias, wbuf.data(), 1u, 0u),
                      static_cast<int16_t>(elem_bytes))
                << ParamTraceLabel(row, entry, index);
        }
    }

    // 2. 全部写完后，再按同样分项读回并与期望图案比对
    for (uint16_t row = 0u; row < tParamApiTableCount; ++row)
    {
        const ST_PARAM_TABLE *entry = &tParamApiTable[row];
        const uint8_t index_count = ParamIndexCount(entry);

        for (uint8_t ordinal = 0u; ordinal < index_count; ++ordinal)
        {
            const uint8_t index = ParamIoIndex(entry, ordinal);
            const uint16_t elem_bytes = ParamElemBytes(entry, index);
            const uint32_t alias = ParaAliasBuild(entry->usParamType, index);

            TraceParamEntry(row, entry, index);
            FillParamWritePattern(wbuf.data(), elem_bytes, row, index);
            std::fill(rbuf.begin(), rbuf.end(), 0u);
            ASSERT_EQ(dc_read_alias(alias, rbuf.data(), 1u, 0u),
                      static_cast<int16_t>(elem_bytes))
                << ParamTraceLabel(row, entry, index);
            EXPECT_TRUE(ExpectParamBuffersEqual(wbuf.data(), rbuf.data(), elem_bytes, row,
                                                entry, index));
        }
    }

    // 3. 分项数 > 1 的条目：PARAM_INDEX_ALL 整项写后再读
    for (uint16_t row = 0u; row < tParamApiTableCount; ++row)
    {
        const ST_PARAM_TABLE *entry = &tParamApiTable[row];
        const uint8_t index_count = ParamIndexCount(entry);

        if (index_count <= 1u)
        {
            continue;
        }

        const uint16_t total_bytes = ParamTotalBytes(entry);
        const uint32_t alias_all = ParaAliasBuild(entry->usParamType, PARAM_INDEX_ALL);

        TraceParamEntry(row, entry, PARAM_INDEX_ALL);
        FillParamWritePattern(wbuf.data(), total_bytes, row, PARAM_INDEX_ALL);
        ASSERT_EQ(dc_write_alias(alias_all, wbuf.data(), index_count, 0u),
                  static_cast<int16_t>(total_bytes))
            << ParamTraceLabel(row, entry, PARAM_INDEX_ALL);

        std::fill(rbuf.begin(), rbuf.end(), 0u);
        ASSERT_EQ(dc_read_alias(alias_all, rbuf.data(), index_count, 0u),
                  static_cast<int16_t>(total_bytes))
            << ParamTraceLabel(row, entry, PARAM_INDEX_ALL);
        EXPECT_TRUE(ExpectParamBuffersEqual(wbuf.data(), rbuf.data(), total_bytes, row, entry,
                                            PARAM_INDEX_ALL));
    }
}

}  // namespace
