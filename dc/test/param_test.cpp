#include <gtest/gtest.h>

extern "C" {
#include "datacenter.h"
}

namespace {

class ParamTest : public ::testing::Test {};

TEST_F(ParamTest, ReadWrite)
{
    uint8_t buf[16];
    uint8_t all[256];
    int16_t ret;
    uint32_t g;

    g = ParaAliasBuild(PARAM_SEASON_SWTIME, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 7);
    EXPECT_EQ(buf[6], 0xFFu);

    buf[0] = 0x26u;
    buf[1] = 0x08u;
    buf[2] = 0x21u;
    buf[3] = 0x00u;
    buf[4] = 0x00u;
    buf[5] = 0x00u;
    buf[6] = 0xAAu;
    ret = WriteAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 7);
    memset(buf, 0, sizeof buf);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 7);
    EXPECT_EQ(buf[6], 0xAAu);

    g = ParaAliasBuild(PARAM_HOLIDAY_DATA, 1);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 12);
    EXPECT_EQ(buf[0], 0xFFu);

    buf[0] = 0x25u;
    buf[1] = 0x01u;
    buf[2] = 0x01u;
    buf[3] = 0x00u;
    buf[4] = 0x00u;
    buf[5] = 0x01u;
    ret = WriteAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 12);

    g = ParaAliasBuild(PARAM_HOLIDAY_DATA, PARAM_INDEX_ALL);
    ret = ReadAliasData(g, all, 20u, 0u);
    ASSERT_EQ(ret, 240);
    EXPECT_EQ(all[12], 0x25u);

    g = ParaAliasBuild(PARAM_HOLIDAY_DATA, 19);
    ret = ReadAliasData(g, buf, 2u, 0u);
    EXPECT_EQ(ret, DC_RET_PARAM_ERR);

    g = ParaAliasBuild(0x40FFu, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    EXPECT_EQ(ret, DC_RET_ALIAS_ERR);

    g = ParaAliasBuild(PARAM_CALIB_DATA, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 12);
    EXPECT_EQ(buf[0], 0xFFu);

    memset(buf, 0xA5u, 12u);
    g = ParaAliasBuild(PARAM_CALIB_DATA, 10);
    ret = WriteAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 12);
    memset(buf, 0, sizeof buf);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 12);
    EXPECT_EQ(buf[0], 0xA5u);

    g = ParaAliasBuild(PARAM_CALIB_DATA, PARAM_INDEX_ALL);
    ret = ReadAliasData(g, all, 20u, 0u);
    ASSERT_EQ(ret, 240);
    EXPECT_EQ(all[10u * 12u], 0xA5u);
    EXPECT_EQ(all[0], 0xFFu);

    g = ParaAliasBuild(PARAM_CALIB_DATA, 19);
    ret = ReadAliasData(g, buf, 2u, 0u);
    EXPECT_EQ(ret, DC_RET_PARAM_ERR);
}

}  // namespace
