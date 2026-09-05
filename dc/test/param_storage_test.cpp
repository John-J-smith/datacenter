#include "param_test_helpers.hpp"

#include <array>
#include <cstring>

extern "C" {
#include "dc_alias_layout.h"
#include "dc_param_layout.h"
#include "dc_test_storage.h"
}

namespace {

TEST_F(ParamTestBase, Write_RamEeBk_MirrorsPrimaryAndBak)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_SEASON_SWTIME);
    const ST_PARAM_BLOCK_TABLE *block;

    ASSERT_NE(entry, nullptr);
    block = &tParamBlockTable[entry->eBlockName];
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);

    EXPECT_EQ(std::memcmp(DcTestStoragePtr() + PARAM_EEPROM_ORIGIN + block->uBlockEeOff +
                              entry->uParamOffset,
                          custom, sizeof(custom)),
              0);
    EXPECT_EQ(std::memcmp(DcTestStoragePtr() + PARAM_EEPROM_ORIGIN + PARAM_EE_BAK_BASE +
                              block->uBlockEeOff + entry->uParamOffset,
                          custom, sizeof(custom)),
              0);
}

TEST_F(ParamTestBase, Write_EeBk_NoSram_PersistsAcrossReinit)
{
    const uint8_t custom[] = {0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u, 0xA6u, 0xA7u};
    std::array<uint8_t, 7u> buf{};
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_DAY_SWTIME);

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(tParamBlockTable[entry->eBlockName].ram, nullptr);

    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_DAY_SWTIME, custom, 1u, 0u), 7);
    DcTestParamReinit();

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_DAY_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
}

TEST_F(ParamTestBase, Write_RamEe_NoBakSlot)
{
    const uint8_t custom[] = {0x31u, 0x32u, 0x33u, 0x34u};
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_UN);
    const ST_PARAM_BLOCK_TABLE *block;
    uint32_t bak_addr;

    ASSERT_NE(entry, nullptr);
    block = &tParamBlockTable[entry->eBlockName];
    EXPECT_EQ(block->ucFlag & FLAG_EEPROM_BAK, 0u);
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_UN, custom, 1u, 0u), 4);

    EXPECT_EQ(std::memcmp(DcTestStoragePtr() + PARAM_EEPROM_ORIGIN + block->uBlockEeOff +
                              entry->uParamOffset,
                          custom, sizeof(custom)),
              0);

    bak_addr = PARAM_EEPROM_ORIGIN + PARAM_EE_BAK_BASE + block->uBlockEeOff;
    EXPECT_EQ(DcTestStoragePtr()[bak_addr], 0xFFu);
}

TEST_F(ParamTestBase, EeOnly_Linkarray_ReadWrite)
{
    uint8_t w[12];
    uint8_t r[12];
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_CALIB_DATA);

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(tParamBlockTable[entry->eBlockName].ram, nullptr);
    EXPECT_EQ(tParamBlockTable[entry->eBlockName].ucFlag, PARAM_STORE_EE);

    for (uint8_t b = 0u; b < 12u; ++b)
    {
        w[b] = static_cast<uint8_t>(0xC0u + b);
    }
    ASSERT_EQ(dc_write_alias(ParaAliasBuild(PARAM_CALIB_DATA, 0u), w, 1u, 0u), 12);
    ASSERT_EQ(dc_read_alias(ParaAliasBuild(PARAM_CALIB_DATA, 7u), r, 1u, 0u), 12);
    std::memset(r, 0, sizeof r);
    ASSERT_EQ(dc_write_alias(ParaAliasBuild(PARAM_CALIB_DATA, 7u), w, 1u, 0u), 12);
    ASSERT_EQ(dc_read_alias(ParaAliasBuild(PARAM_CALIB_DATA, 7u), r, 1u, 0u), 12);
    EXPECT_EQ(std::memcmp(w, r, sizeof w), 0);
}

}  // namespace
