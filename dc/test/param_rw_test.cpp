#include "param_test_helpers.hpp"

#include <algorithm>

namespace {

// 测试内容：tParamApiTable 全部参变量、全部分项 index 读写回读（经 DC_ALIAS_PARAM_* 别名）
TEST_F(ParamTestBase, AllParams_ReadWrite)
{
    std::vector<uint8_t> wbuf(MakeParamIoBuffer());
    std::vector<uint8_t> rbuf(MakeParamIoBuffer());

    for (uint16_t row = 0u; row < tParamApiTableCount; ++row)
    {
        const ST_PARAM_TABLE *entry = &tParamApiTable[row];
        const uint8_t index_count = ParamIndexCount(entry);

        for (uint8_t index = 0u; index < index_count; ++index)
        {
            const uint16_t elem_bytes = ParamElemBytes(entry, index);
            const uint32_t alias = ParaAliasBuild(entry->eParamType, index);
            
            TraceParamEntry(row, entry, index);
            FillParamWritePattern(wbuf.data(), elem_bytes, row, index);
            ASSERT_EQ(dc_write_alias(alias, wbuf.data(), 1u, 0u),
                      static_cast<int16_t>(elem_bytes)) << "alias: " << alias;
        }
    }

    for (uint16_t row = 0u; row < tParamApiTableCount; ++row)
    {
        const ST_PARAM_TABLE *entry = &tParamApiTable[row];
        const uint8_t index_count = ParamIndexCount(entry);

        for (uint8_t index = 0u; index < index_count; ++index)
        {
            const uint16_t elem_bytes = ParamElemBytes(entry, index);
            const uint32_t alias = ParaAliasBuild(entry->eParamType, index);

            TraceParamEntry(row, entry, index);
            FillParamWritePattern(wbuf.data(), elem_bytes, row, index);
            std::fill(rbuf.begin(), rbuf.end(), 0u);
            ASSERT_EQ(dc_read_alias(alias, rbuf.data(), 1u, 0u),
                      static_cast<int16_t>(elem_bytes));
            EXPECT_EQ(std::memcmp(wbuf.data(), rbuf.data(), elem_bytes), 0);
        }
    }

    for (uint16_t row = 0u; row < tParamApiTableCount; ++row)
    {
        const ST_PARAM_TABLE *entry = &tParamApiTable[row];
        const uint8_t index_count = ParamIndexCount(entry);

        if (index_count <= 1u)
        {
            continue;
        }

        const uint16_t total_bytes = ParamTotalBytes(entry);
        const uint32_t alias_all = ParaAliasBuild(entry->eParamType, PARAM_INDEX_ALL);

        TraceParamEntry(row, entry, PARAM_INDEX_ALL);
        FillParamWritePattern(wbuf.data(), total_bytes, row, PARAM_INDEX_ALL);
        ASSERT_EQ(dc_write_alias(alias_all, wbuf.data(), index_count, 0u),
                  static_cast<int16_t>(total_bytes));

        std::fill(rbuf.begin(), rbuf.end(), 0u);
        ASSERT_EQ(dc_read_alias(alias_all, rbuf.data(), index_count, 0u),
                  static_cast<int16_t>(total_bytes));
        EXPECT_EQ(std::memcmp(wbuf.data(), rbuf.data(), total_bytes), 0);
    }
}

}  // namespace
