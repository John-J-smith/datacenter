#include "param_test_helpers.hpp"

#include <array>
#include <cstring>

extern "C" {
#include "dc_alias_layout.h"
#include "dc_param_layout.h"
#include "dc_test_storage.h"
}

namespace {

// 测试内容：RAM_EE_BK 写入后主槽与 bak 槽数据一致
TEST_F(ParamTestBase, Write_RamEeBk_MirrorsPrimaryAndBak)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_SEASON_SWTIME);
    const ST_PARAM_BLOCK_TABLE *block;

    ASSERT_NE(entry, nullptr);
    block = &tParamBlockTable[entry->ucBlockName];
    // 1. 写 SEASON_SWTIME 并读回
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);

    // 2. 主槽与 PARAM_EE_BAK_BASE + 主槽偏移 内容相同
    EXPECT_EQ(std::memcmp(DcTestStoragePtr() + PARAM_EEPROM_ORIGIN + block->ulBlockEeOff +
                              entry->ucParamOffset,
                          custom, sizeof(custom)),
              0);
    EXPECT_EQ(std::memcmp(DcTestStoragePtr() + PARAM_EEPROM_ORIGIN + PARAM_EE_BAK_BASE +
                              block->ulBlockEeOff + entry->ucParamOffset,
                          custom, sizeof(custom)),
              0);
}

// 测试内容：EE_BK 无 SRAM，写入后 noinit 仍能从 EE 读回
TEST_F(ParamTestBase, Write_EeBk_NoSram_PersistsAcrossReinit)
{
    const uint8_t custom[] = {0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u, 0xA6u, 0xA7u};
    std::array<uint8_t, 7u> buf{};
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_DAY_SWTIME);

    ASSERT_NE(entry, nullptr);
    // 1. 块 ram 为空
    EXPECT_EQ(tParamBlockTable[entry->ucBlockName].pucRam, nullptr);

    // 2. 写入后 DcTestParamReinit，再读仍为写入值
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_DAY_SWTIME, custom, 1u, 0u), 7);
    DcTestParamReinit();

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_DAY_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
}

// 测试内容：RAM_EE 无 BAK 标志，写入只落主槽、bak 地址仍为擦除填充
TEST_F(ParamTestBase, Write_RamEe_NoBakSlot)
{
    const uint8_t custom[] = {0x31u, 0x32u, 0x33u, 0x34u};
    /* PARAM_IMAX is RAM_EE (no BAK). PARAM_UN is now RAM_EE_BK. */
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_IMAX);
    const ST_PARAM_BLOCK_TABLE *block;
    uint32_t bak_addr;

    ASSERT_NE(entry, nullptr);
    block = &tParamBlockTable[entry->ucBlockName];
    EXPECT_EQ(block->ucFlag & FLAG_EEPROM_BAK, 0u);
    // 1. 写 PARAM_IMAX，主槽为写入值
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_IMAX, custom, 1u, 0u), 4);

    EXPECT_EQ(std::memcmp(DcTestStoragePtr() + PARAM_EEPROM_ORIGIN + block->ulBlockEeOff +
                              entry->ucParamOffset,
                          custom, sizeof(custom)),
              0);

    // 2. BAK_BASE + 主槽偏移处仍为 0xFF（无备份槽）
    bak_addr = PARAM_EEPROM_ORIGIN + PARAM_EE_BAK_BASE + block->ulBlockEeOff;
    EXPECT_EQ(DcTestStoragePtr()[bak_addr], 0xFFu);
}

// 测试内容：参变量 EE 写入失败时 dc_write 返回错误
TEST_F(ParamTestBase, Write_StorageFail_ReturnsParamErr)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    DcTestStorageFailNextWrites(4u);
    EXPECT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), DC_RET_PARAM_ERR);
}

// 测试内容：参变量类入口拒绝空缓冲
TEST_F(ParamTestBase, DirectApi_NullBufferWithLength)
{
    std::array<uint8_t, 7u> buf{};

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(dc_read_param(DC_ALIAS_PARAM_SEASON_SWTIME, 0, 1u, 0u), DC_RET_PARAM_ERR);
    EXPECT_EQ(dc_write_param(DC_ALIAS_PARAM_SEASON_SWTIME, 0, 1u, 0u), DC_RET_PARAM_ERR);
}

// 测试内容：仅 EE 的 LINKARRAY 按记录 index 读写回读
TEST_F(ParamTestBase, EeOnly_Linkarray_ReadWrite)
{
    uint8_t w[12];
    uint8_t r[12];
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_CALIB_DATA);

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(tParamBlockTable[entry->ucBlockName].pucRam, nullptr);
    EXPECT_EQ(tParamBlockTable[entry->ucBlockName].ucFlag, PARAM_STORE_EE);

    for (uint8_t b = 0u; b < 12u; ++b)
    {
        w[b] = static_cast<uint8_t>(0xC0u + b);
    }
    // 1. 写记录 0，再写记录 7 并读回记录 7
    ASSERT_EQ(dc_write_alias(ParaAliasBuild(PARAM_CALIB_DATA, 0u), w, 1u, 0u), 12);
    ASSERT_EQ(dc_read_alias(ParaAliasBuild(PARAM_CALIB_DATA, 7u), r, 1u, 0u), 12);
    std::memset(r, 0, sizeof r);
    ASSERT_EQ(dc_write_alias(ParaAliasBuild(PARAM_CALIB_DATA, 7u), w, 1u, 0u), 12);
    ASSERT_EQ(dc_read_alias(ParaAliasBuild(PARAM_CALIB_DATA, 7u), r, 1u, 0u), 12);
    // 2. 记录 7 与写入图案一致
    EXPECT_EQ(std::memcmp(w, r, sizeof w), 0);
}

}  // namespace

