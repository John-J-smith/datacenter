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
            EXPECT_EQ(off, prev + PARAM_BLOCK_EE_SLOT_LEN);
        }
        prev = off;
    }
    if (prev != PARAM_BLOCK_NULL_EE_OFF)
    {
        EXPECT_EQ(PARAM_EE_TOTAL, prev + PARAM_BLOCK_EE_SLOT_LEN);
    }
}

TEST_F(ParamLayoutTest, BlockTableAddresses)
{
    EXPECT_EQ(tParamBlockTable[0].uBlockEeOff, PARAM_LAYOUT_BLOCK_0_EE_OFF);
    EXPECT_EQ(tParamBlockTable[1].uBlockEeOff, PARAM_LAYOUT_BLOCK_1_EE_OFF);
    EXPECT_NE(tParamBlockTable[0].ram, nullptr);
    EXPECT_NE(tParamBlockTable[PARAM_LAYOUT_BLOCK_COUNT - 1u].ram, nullptr);
    EXPECT_EQ(PARAM_EEPROM_ORIGIN, static_cast<uint32_t>(PARAM_EEPROM_BASE));
}

TEST_F(ParamLayoutTest, ApiTableRowCount)
{
    EXPECT_EQ(tParamApiTableCount, PARAM_LAYOUT_ITEM_COUNT);
}

}  // namespace
