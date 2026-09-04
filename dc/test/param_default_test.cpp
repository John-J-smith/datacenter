#include "param_test_helpers.hpp"

#include <array>
#include <cstring>

extern "C" {
#include "dc_alias_layout.h"
#include "dc_param_layout.h"
extern const uint8_t g_default_PARAM_SEASON_SWTIME[];
extern const uint8_t g_default_PARAM_DAY_SWTIME[];
extern const uint8_t g_default_PARAM_FEE_SWTIME[];
extern const uint8_t g_default_PARAM_LADDER_SWTIME[];
}

namespace {

// 测试内容：catalog 注册默认值的参数，冷启动首次读返回 g_default_* 内容
TEST_F(ParamTestBase, FirstRead_CatalogDefaultParam_ReturnsDefaultBytes)
{
    std::array<uint8_t, 7u> buf{};

    // 1. dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME) 触发 param_ensure_init
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);

    // 2. 断言读回字节与 g_default_PARAM_SEASON_SWTIME 一致
    EXPECT_EQ(std::memcmp(buf.data(), g_default_PARAM_SEASON_SWTIME, buf.size()), 0);
}

// 测试内容：pDefault 为 NULL 的参数，冷启动首次读返回 0xFF 填充
TEST_F(ParamTestBase, FirstRead_NoDefaultParam_Returns0xFF)
{
    std::array<uint8_t, 4u> buf{};

    // 1. dc_read_alias(DC_ALIAS_PARAM_UN)
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_UN, buf.data(), 1u, 0u), 4);

    // 2. 断言各字节均为 0xFF
    for (uint8_t b : buf)
    {
        EXPECT_EQ(b, 0xFFu);
    }
}

// 测试内容：同块内混排——有默认值的项用 catalog，无默认值的项为 0xFF
TEST_F(ParamTestBase, FirstRead_SharedBlock_MixedDefaultsAndFill)
{
    std::array<uint8_t, 7u> swtime{};
    std::array<uint8_t, 4u> un{};

    // 1. 读 PARAM_LADDER_SWTIME（块 2，有 pDefault）
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_LADDER_SWTIME, swtime.data(), 1u, 0u), 7);

    // 2. 读 PARAM_UN（同块，pDefault 为 NULL）
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_UN, un.data(), 1u, 0u), 4);

    // 3. 分别断言默认表与 0xFF 填充
    EXPECT_EQ(std::memcmp(swtime.data(), g_default_PARAM_LADDER_SWTIME, swtime.size()), 0);
    for (uint8_t b : un)
    {
        EXPECT_EQ(b, 0xFFu);
    }
}

// 测试内容：PARAM_ITEM_DEFAULTS 中全部 swtime 参数首次读与 layout 默认表一致
TEST_F(ParamTestBase, AllCatalogDefaultParams_MatchLayoutTables)
{
    struct default_case {
        uint32_t alias;
        const uint8_t *expected;
        uint16_t len;
    };

    const default_case cases[] = {
        {DC_ALIAS_PARAM_SEASON_SWTIME, g_default_PARAM_SEASON_SWTIME, 7u},
        {DC_ALIAS_PARAM_DAY_SWTIME, g_default_PARAM_DAY_SWTIME, 7u},
        {DC_ALIAS_PARAM_FEE_SWTIME, g_default_PARAM_FEE_SWTIME, 7u},
        {DC_ALIAS_PARAM_LADDER_SWTIME, g_default_PARAM_LADDER_SWTIME, 7u},
    };

    // 1. 遍历 SEASON / DAY / FEE / LADDER 四个别名
    for (const default_case &c : cases)
    {
        std::vector<uint8_t> buf(c.len, 0u);

        SCOPED_TRACE("alias=" + std::to_string(c.alias));

        // 2. dc_read_alias 后与对应 g_default_* 比较
        ASSERT_EQ(dc_read_alias(c.alias, buf.data(), 1u, 0u), static_cast<int16_t>(c.len));
        EXPECT_EQ(std::memcmp(buf.data(), c.expected, c.len), 0);
    }
}

// 测试内容：默认值初始化只执行一次，写入后读回用户数据而非 catalog 默认
TEST_F(ParamTestBase, AfterWrite_ReadReturnsWrittenValueNotDefault)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    // 1. 首次读触发 param_ensure_init
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);

    // 2. dc_write_alias 写入自定义 pattern
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);

    // 3. 再读断言与写入一致（非 g_default_*）
    std::fill(buf.begin(), buf.end(), 0u);
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
}

// 测试内容：noinit 下块 CRC 有效时，重新 init 保留 RAM 内容
TEST_F(ParamTestBase, Noinit_ValidBlockCrc_KeepsRamAcrossReinit)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    // 1. 冷启动读默认并写入自定义值（写路径会刷新块 CRC）
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);

    // 2. 仅清除 init 标志，模拟 noinit 软复位
    DcTestParamReinit();

    // 3. 再读应仍为写入值
    std::fill(buf.begin(), buf.end(), 0u);
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), custom, sizeof(custom)), 0);
}

// 测试内容：noinit 下块 CRC 失效时，重新 init 恢复 catalog 默认
TEST_F(ParamTestBase, Noinit_BadBlockCrc_RestoresCatalogDefault)
{
    const uint8_t custom[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    std::array<uint8_t, 7u> buf{};

    // 1. 写入自定义值
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    ASSERT_EQ(dc_write_alias(DC_ALIAS_PARAM_SEASON_SWTIME, custom, 1u, 0u), 7);

    // 2. 破坏该参数所在块的 CRC，再仅清除 init 标志
    const ST_PARAM_TABLE *entry = ParamFindEntry(PARAM_SEASON_SWTIME);
    ASSERT_NE(entry, nullptr);
    ParamCorruptBlockCrc(entry->eBlockName);
    DcTestParamReinit();

    // 3. 再读应恢复为 g_default_*
    std::fill(buf.begin(), buf.end(), 0u);
    ASSERT_EQ(dc_read_alias(DC_ALIAS_PARAM_SEASON_SWTIME, buf.data(), 1u, 0u), 7);
    EXPECT_EQ(std::memcmp(buf.data(), g_default_PARAM_SEASON_SWTIME, buf.size()), 0);
}

}  // namespace
