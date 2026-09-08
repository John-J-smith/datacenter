#include "param_test_helpers.hpp"

#include <algorithm>

namespace {

// 测试内容：LIST attrib 按 xy / xF / 0xFF 推导偏移与长度，非法分项失败
TEST_F(ParamTestBase, LookupLeafGroupAndAll)
{
    static const uint8_t attr[] = {
        (uint8_t)DATATYPE_LIST, 4u,
        0x00u, 2u,
        0x01u, 4u,
        0x10u, 2u,
        0x11u, 4u
    };
    uint16_t off;
    uint16_t len;

    // 1. 叶子 00/01 与组 0F
    ASSERT_NE(param_attr_list_lookup(attr, 0x00u, &off, &len), 0);
    EXPECT_EQ(off, 0u);
    EXPECT_EQ(len, 2u);

    ASSERT_NE(param_attr_list_lookup(attr, 0x01u, &off, &len), 0);
    EXPECT_EQ(off, 2u);
    EXPECT_EQ(len, 4u);

    ASSERT_NE(param_attr_list_lookup(attr, 0x0Fu, &off, &len), 0);
    EXPECT_EQ(off, 0u);
    EXPECT_EQ(len, 6u);

    // 2. 叶子 10/11 与组 1F、整表 0xFF
    ASSERT_NE(param_attr_list_lookup(attr, 0x10u, &off, &len), 0);
    EXPECT_EQ(off, 6u);
    EXPECT_EQ(len, 2u);

    ASSERT_NE(param_attr_list_lookup(attr, 0x1Fu, &off, &len), 0);
    EXPECT_EQ(off, 6u);
    EXPECT_EQ(len, 6u);

    ASSERT_NE(param_attr_list_lookup(attr, PARAM_INDEX_ALL, &off, &len), 0);
    EXPECT_EQ(off, 0u);
    EXPECT_EQ(len, 12u);

    // 3. 不存在的 0x02 失败；叶子 ordinal 映射 xy
    EXPECT_EQ(param_attr_list_lookup(attr, 0x02u, &off, &len), 0);
    EXPECT_EQ(param_attr_list_leaf_xy(attr, 0u), 0x00u);
    EXPECT_EQ(param_attr_list_leaf_xy(attr, 2u), 0x10u);
}

// 测试内容：catalog 中每个 LIST：按叶子写入，再按 xF / 0xFF 回读拼接结果
TEST_F(ParamTestBase, AllListParams_LeafGroupAll)
{
    std::vector<uint8_t> wbuf(MakeParamIoBuffer());
    std::vector<uint8_t> rbuf(MakeParamIoBuffer());
    unsigned list_n = 0u;

    for (uint16_t row = 0u; row < tParamApiTableCount; ++row)
    {
        const ST_PARAM_TABLE *entry = &tParamApiTable[row];
        const uint8_t *attr;
        const uint8_t leaf_n = ParamIndexCount(entry);
        const uint16_t total = ParamTotalBytes(entry);
        std::vector<uint8_t> expected; /* 按叶子写入内容拼出的整段期望值 */
        uint8_t last_g;
        uint8_t i;

        if (param_attr_type(entry) != (uint8_t)DATATYPE_LIST)
        {
            continue;
        }
        list_n++;
        attr = entry->pucAttr;
        ASSERT_NE(attr, nullptr) << ParamTraceLabel(row, entry, 0u);
        ASSERT_GT(leaf_n, 0u) << ParamTraceLabel(row, entry, 0u);
        expected.assign(total, 0u);

        // 1. 按叶子写入，同时拼出整段期望值
        for (i = 0u; i < leaf_n; ++i)
        {
            const uint8_t xy = param_attr_list_leaf_xy(attr, i);
            uint16_t off;
            uint16_t len;
            const uint32_t alias = ParaAliasBuild(entry->usParamType, xy);

            ASSERT_NE(param_attr_list_lookup(attr, xy, &off, &len), 0)
                << ParamTraceLabel(row, entry, xy);
            ASSERT_LE(static_cast<unsigned>(off) + len, static_cast<unsigned>(total))
                << ParamTraceLabel(row, entry, xy);
            FillParamWritePattern(wbuf.data(), len, row, xy);
            TraceParamEntry(row, entry, xy);
            ASSERT_EQ(dc_write_alias(alias, wbuf.data(), 1u, 0u),
                      static_cast<int16_t>(len))
                << ParamTraceLabel(row, entry, xy);
            std::memcpy(expected.data() + off, wbuf.data(), len);
        }

        // 2. 每组读 xF，与期望值对应区间比对
        last_g = 0xFFu;
        for (i = 0u; i < leaf_n; ++i)
        {
            const uint8_t xy = param_attr_list_leaf_xy(attr, i);
            const uint8_t g = static_cast<uint8_t>(xy >> 4);
            uint8_t xf;
            uint16_t off;
            uint16_t len;
            uint32_t alias;

            if (g == last_g)
            {
                continue;
            }
            last_g = g;
            xf = static_cast<uint8_t>((static_cast<unsigned>(g) << 4) | 0x0Fu);
            alias = ParaAliasBuild(entry->usParamType, xf);
            ASSERT_NE(param_attr_list_lookup(attr, xf, &off, &len), 0)
                << ParamTraceLabel(row, entry, xf);
            TraceParamEntry(row, entry, xf);
            std::fill(rbuf.begin(), rbuf.end(), 0u);
            ASSERT_EQ(dc_read_alias(alias, rbuf.data(), 1u, 0u),
                      static_cast<int16_t>(len))
                << ParamTraceLabel(row, entry, xf);
            EXPECT_TRUE(ExpectParamBuffersEqual(expected.data() + off, rbuf.data(), len, row,
                                                entry, xf));
        }

        // 3. PARAM_INDEX_ALL 读整段
        {
            const uint32_t alias_all = ParaAliasBuild(entry->usParamType, PARAM_INDEX_ALL);

            TraceParamEntry(row, entry, PARAM_INDEX_ALL);
            std::fill(rbuf.begin(), rbuf.end(), 0u);
            ASSERT_EQ(dc_read_alias(alias_all, rbuf.data(), 1u, 0u),
                      static_cast<int16_t>(total))
                << ParamTraceLabel(row, entry, PARAM_INDEX_ALL);
            EXPECT_TRUE(ExpectParamBuffersEqual(expected.data(), rbuf.data(), total, row, entry,
                                                PARAM_INDEX_ALL));
        }

        // 4. 叶子 usLen!=1 返回 DC_RET_PARAM_ERR
        {
            const uint8_t xy0 = param_attr_list_leaf_xy(attr, 0u);

            EXPECT_EQ(dc_write_alias(ParaAliasBuild(entry->usParamType, xy0), wbuf.data(), 2u,
                                     0u),
                      DC_RET_PARAM_ERR)
                << ParamTraceLabel(row, entry, xy0);
        }
    }

    ASSERT_GE(list_n, 1u) << "catalog has no LIST item";
}

}  // namespace
