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

TEST_F(ParamLayoutTest, EeMapContiguous)
{
    uint32_t prev = PARAM_BLOCK_NULL_EE_OFF;

    EXPECT_EQ(PARAM_LAYOUT_BLOCK_0_EE_OFF, 0u);
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
    if (prev != PARAM_BLOCK_NULL_EE_OFF)
    {
        EXPECT_EQ(PARAM_EE_TOTAL, PARAM_EE_TOTAL_ALIGN(prev + PARAM_BLOCK_SIZE));
    }
}

TEST_F(ParamLayoutTest, BlockTableAddresses)
{
    EXPECT_EQ(tParamBlockTable[0].uBlockEeOff, PARAM_LAYOUT_BLOCK_0_EE_OFF);
    EXPECT_EQ(PARAM_EEPROM_ORIGIN, static_cast<uint32_t>(PARAM_EEPROM_BASE));
    EXPECT_EQ(PARAM_EE_BAK_BASE, PARAM_EE_TOTAL);
}

TEST_F(ParamLayoutTest, StoreFlagsGrouped)
{
    uint8_t prev = 0u;
    int have = 0;

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

TEST_F(ParamLayoutTest, BakOffDerivedFromPrimary)
{
    for (uint16_t bi = 0u; bi < tParamBlockTableCount; ++bi)
    {
        if ((tParamBlockTable[bi].ucFlag & FLAG_EEPROM_BAK) == 0u)
        {
            continue;
        }
        EXPECT_EQ(PARAM_EE_BAK_BASE + tParamBlockTable[bi].uBlockEeOff,
                  PARAM_EE_BAK_BASE + tParamBlockTable[bi].uBlockEeOff);
        EXPECT_LT(tParamBlockTable[bi].uBlockEeOff, PARAM_EE_BAK_SPAN);
    }
}

TEST_F(ParamLayoutTest, ApiTableRowCount)
{
    EXPECT_EQ(tParamApiTableCount, PARAM_LAYOUT_ITEM_COUNT);
}

}  // namespace
