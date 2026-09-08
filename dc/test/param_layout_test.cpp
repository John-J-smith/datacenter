#include <gtest/gtest.h>

extern "C" {
#include "dc_storage_cfg.h"
#include "dc_param.h"
#include "dc_param_layout.h"

extern const ST_PARAM_BLOCK_TABLE tParamBlockTable[];
extern const uint32_t PARAM_EEPROM_ORIGIN;
}

namespace {

class ParamLayoutTest : public ::testing::Test {};

// 测试内容：主槽 EE 偏移按 PARAM_BLOCK_SIZE 连续，末块对齐 PARAM_EE_TOTAL（8.2.1）
TEST_F(ParamLayoutTest, EeMapContiguous)
{
    uint32_t prev = PARAM_BLOCK_NULL_EE_OFF;

    // 1. 块 0 主槽起点为 0
    EXPECT_EQ(PARAM_LAYOUT_BLOCK_0_EE_OFF, 0u);
    // 2. 遍历 tParamBlockTable 有效主槽，相邻偏移差一块
    for (uint16_t bi = 0u; bi < PARAM_LAYOUT_BLOCK_COUNT; ++bi)
    {
        const uint32_t off = tParamBlockTable[bi].uBlockEeOff;

        if (off == PARAM_BLOCK_NULL_EE_OFF)
        {
            continue;
        }
        if (prev == PARAM_BLOCK_NULL_EE_OFF)
        {
            EXPECT_EQ(off, 0u);
        }
        else
        {
            EXPECT_EQ(off, prev + PARAM_BLOCK_SIZE);
        }
        prev = off;
    }
    // 3. 末主槽 + 一块后按页对齐得到 PARAM_EE_TOTAL
    if (prev != PARAM_BLOCK_NULL_EE_OFF)
    {
        EXPECT_EQ(PARAM_EE_TOTAL, PARAM_EE_TOTAL_ALIGN(prev + PARAM_BLOCK_SIZE));
    }
}

// 测试内容：块表与 ORIGIN / BAK_BASE 宏一致（8.2.1）
TEST_F(ParamLayoutTest, BlockTableAddresses)
{
    // 1. 块 0 与 PARAM_LAYOUT_BLOCK_0_EE_OFF、ORIGIN、BAK_BASE 对齐
    EXPECT_EQ(tParamBlockTable[0].uBlockEeOff, PARAM_LAYOUT_BLOCK_0_EE_OFF);
    EXPECT_EQ(PARAM_EEPROM_ORIGIN, static_cast<uint32_t>(PARAM_EEPROM_BASE));
    EXPECT_EQ(PARAM_EE_BAK_BASE, PARAM_EE_TOTAL);
}

// 测试内容：块表存储类型按 RAM_EE_BK → EE_BK → RAM_EE → EE 分段，SRAM 指针与 FLAG 一致
TEST_F(ParamLayoutTest, StoreFlagsGrouped)
{
    uint8_t prev = 0u;
    int have = 0;

    // 1. 遍历块表：flags 只允许规定邻接跳变；有 SRAM 则 ram 非空
    for (uint16_t bi = 0u; bi < tParamBlockTableCount; ++bi)
    {
        const uint8_t f = tParamBlockTable[bi].ucFlag;
        if (have != 0)
        {
            EXPECT_TRUE((f == prev) ||
                        ((prev == PARAM_STORE_RAM_EE_BK) && (f == PARAM_STORE_EE_BK)) ||
                        ((prev == PARAM_STORE_EE_BK) && (f == PARAM_STORE_RAM_EE)) ||
                        ((prev == PARAM_STORE_RAM_EE) && (f == PARAM_STORE_EE)))
                << "block " << bi;
        }
        if ((f & FLAG_SRAM) != 0u)
        {
            EXPECT_NE(tParamBlockTable[bi].ram, nullptr);
        }
        else
        {
            EXPECT_EQ(tParamBlockTable[bi].ram, nullptr);
        }
        prev = f;
        have = 1;
    }
}

// 测试内容：带 BAK 的主槽备份地址 = PARAM_EE_BAK_BASE + 主槽 uBlockEeOff，且落入 bak 宏表
TEST_F(ParamLayoutTest, BakOffDerivedFromPrimary)
{
    static const uint32_t bak_macro[] = { PARAM_LAYOUT_BAK_OFFS };
    unsigned k = 0u;

    // 1. 每个 FLAG_EEPROM_BAK 主槽：bak = BAK_BASE + 主槽偏移，等于对应 EE_BK_OFF 宏
    for (uint16_t bi = 0u; bi < tParamBlockTableCount; ++bi)
    {
        uint32_t bak;

        if ((tParamBlockTable[bi].ucFlag & FLAG_EEPROM_BAK) == 0u)
        {
            continue;
        }
        ASSERT_LT(k, (unsigned)(sizeof(bak_macro) / sizeof(bak_macro[0]))) << "block " << bi;
        bak = PARAM_EE_BAK_BASE + tParamBlockTable[bi].uBlockEeOff;
        EXPECT_EQ(bak, bak_macro[k]) << "block " << bi;
        EXPECT_LT(bak, PARAM_EE_MAP_END) << "block " << bi;
        k++;
    }
    // 2. 带 BAK 的主槽个数与 EE_BK_OFF 宏个数一致
    EXPECT_EQ(k, (unsigned)(sizeof(bak_macro) / sizeof(bak_macro[0])));
}

// 测试内容：API 表行数与 PARAM_LAYOUT_ITEM_COUNT 一致（8.2.1）
TEST_F(ParamLayoutTest, ApiTableRowCount)
{
    // 1. tParamApiTableCount == PARAM_LAYOUT_ITEM_COUNT
    EXPECT_EQ(tParamApiTableCount, PARAM_LAYOUT_ITEM_COUNT);
}

}  // namespace
