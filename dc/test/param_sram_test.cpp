#include "param_test_helpers.hpp"

#include <array>
#include <cstring>

extern "C" {
#include "dc_alias_layout.h"
#include "dc_param_layout.h"
}

namespace {

class ParamSramTest : public ParamTestBase {};

TEST_F(ParamSramTest, Init_BadCrcEvenIfMagicGood_RestoresFromEe)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);
    ASSERT_NE(DcTestParamSramOk(), 0u);

    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_SEASON_SWTIME);
    ASSERT_NE(entry, nullptr);
    ParamCorruptBlockCrc(entry->ucBlockName);
    DcTestParamReinit();

    std::fill(buf.begin(), buf.end(), 0u);
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
    EXPECT_NE(DcTestParamSramOk(), 0u);
}

TEST_F(ParamSramTest, Init_GoodCrcBadMagic_KeepsRamAndRestamps)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);
    DcTestParamCorruptSramMagic();
    DcTestParamReinit();

    std::fill(buf.begin(), buf.end(), 0u);
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
    EXPECT_NE(DcTestParamSramOk(), 0u);
}

TEST_F(ParamSramTest, Runtime_BadCrcMagicGood_KeepsRam)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);

    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_SEASON_SWTIME);
    ASSERT_NE(entry, nullptr);
    ParamCorruptBlockCrc(entry->ucBlockName);
    ASSERT_NE(DcTestParamSramOk(), 0u);

    std::fill(buf.begin(), buf.end(), 0u);
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
}

TEST_F(ParamSramTest, Runtime_BadMagicCrcGood_KeepsRamAndRestamps)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);
    DcTestParamCorruptSramMagic();
    ASSERT_EQ(DcTestParamSramOk(), 0u);

    std::fill(buf.begin(), buf.end(), 0u);
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
    EXPECT_NE(DcTestParamSramOk(), 0u);
}

}  // namespace
