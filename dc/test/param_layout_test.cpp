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
    EXPECT_EQ(PARAM_LAYOUT_BLOCK_0_EE_OFF, 0u);
    EXPECT_EQ(PARAM_LAYOUT_BLOCK_1_EE_OFF,
              PARAM_LAYOUT_BLOCK_0_EE_OFF + PARAM_BLOCK_BYTES_MAX);
    EXPECT_EQ(PARAM_LAYOUT_BLOCK_2_EE_OFF,
              PARAM_LAYOUT_BLOCK_1_EE_OFF + PARAM_BLOCK_BYTES_MAX);
    EXPECT_EQ(PARAM_LAYOUT_BLOCK_9_EE_OFF,
              PARAM_LAYOUT_BLOCK_8_EE_OFF + PARAM_BLOCK_BYTES_MAX);
    EXPECT_EQ(PARAM_EE_TOTAL,
              PARAM_LAYOUT_BLOCK_9_EE_OFF + PARAM_BLOCK_BYTES_MAX);
}

TEST_F(ParamLayoutTest, BlockTableAddresses)
{
    EXPECT_EQ(tParamBlockTable[0].uBlockEeOff, PARAM_LAYOUT_BLOCK_0_EE_OFF);
    EXPECT_EQ(tParamBlockTable[1].uBlockEeOff, PARAM_LAYOUT_BLOCK_1_EE_OFF);
    EXPECT_NE(tParamBlockTable[0].ram, nullptr);
    EXPECT_NE(tParamBlockTable[9].ram, nullptr);
    EXPECT_EQ(PARAM_EEPROM_ORIGIN, static_cast<uint32_t>(PARAM_EEPROM_BASE));
    EXPECT_EQ(PARAM_EEPROM_ORIGIN + tParamBlockTable[5].uBlockEeOff,
              static_cast<uint32_t>(PARAM_EEPROM_BASE) +
                  static_cast<uint32_t>(PARAM_LAYOUT_BLOCK_5_EE_OFF));
}

}  // namespace
