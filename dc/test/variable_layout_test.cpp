#include "variable_test_helpers.hpp"

extern "C" {
#include "dc_variable_layout.h"
#include "dc_variable_cfg.h"
}

namespace {

#define VAR_CFG_COUNT_ROW(...) + 1

constexpr uint16_t VarCfgCatalogCount()
{
    return static_cast<uint16_t>(
        0 VAR_LIST_A(VAR_CFG_COUNT_ROW) VAR_LIST_B(VAR_CFG_COUNT_ROW)
          VAR_LIST_C(VAR_CFG_COUNT_ROW) VAR_LIST_D(VAR_CFG_COUNT_ROW));
}

}  // namespace

// 测试内容：EE 槽位地址与 A/B/D 连续映射（8.2.1）
TEST_F(VariableTestBase, EeSlotAddresses)
{
    // 1. Seed A 区 PWR_ON_0
    SeedAClassEe(0x99u);

    // 2. 断言 VAR_*_EEPROM_BASE、VariableEeSlotAddr 与 VAR_EE_BACKUP_BANKS 分支
    EXPECT_EQ(VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_0), static_cast<uint32_t>(VAR_EEPROM_BASE));
    EXPECT_EQ(VAR_A_EEPROM_BASE, static_cast<uint32_t>(VAR_EEPROM_BASE));
    EXPECT_EQ(VAR_B_EEPROM_BASE,
              static_cast<uint32_t>(VAR_EEPROM_BASE) + static_cast<uint32_t>(VAR_A_EE_TOTAL));
    EXPECT_EQ(VAR_D_EEPROM_BASE,
              static_cast<uint32_t>(VAR_EEPROM_BASE) + static_cast<uint32_t>(VAR_A_EE_TOTAL) +
                  static_cast<uint32_t>(VAR_B_EE_TOTAL));
    EXPECT_EQ(VariableEeSlotAddr(VAR_EE_SLOT_D_DATA), VAR_D_EEPROM_BASE);
#if (VAR_EE_BACKUP_BANKS >= 2)
    EXPECT_EQ(VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_1),
              static_cast<uint32_t>(VAR_EEPROM_BASE) + static_cast<uint32_t>(VAR_A_EE_PWR_ON_1));
    EXPECT_EQ(VAR_A_EE_TOTAL, static_cast<uint16_t>(3u * VAR_A_END_ADDR));
#else
    EXPECT_EQ(VAR_A_EE_TOTAL, static_cast<uint16_t>(2u * VAR_A_END_ADDR));
#endif
}

// 测试内容：layout struct 大小与 VAR_*_END_ADDR 一致（8.2.1）
TEST_F(VariableTestBase, StructSizesMatchEndAddr)
{
    // 1. 比较 sizeof(var_layout_a/b/c/d_t) 与 VAR_A/B/C/D_END_ADDR
    EXPECT_EQ(sizeof(var_layout_a_t), static_cast<size_t>(VAR_A_END_ADDR));
    EXPECT_EQ(sizeof(var_layout_b_t), static_cast<size_t>(VAR_B_END_ADDR));
    EXPECT_EQ(sizeof(var_layout_c_t), static_cast<size_t>(VAR_C_END_ADDR));
    EXPECT_EQ(sizeof(var_layout_d_t), static_cast<size_t>(VAR_D_END_ADDR));
}

// 测试内容：映射表行数与 cfg 清单条目数一致（8.2.1）
TEST_F(VariableTestBase, ApiTableRowCount)
{
    // 1. 断言 tVariableApiTableCount 与 VAR_LIST_A/B/C/D 展开条目数一致
    EXPECT_EQ(tVariableApiTableCount, VarCfgCatalogCount());
}

// 测试内容：每行偏移+长度不越对应分区 END_ADDR（8.2.1）
TEST_F(VariableTestBase, ApiTableOffsetsInBounds)
{
    // 1. 遍历 tVariableApiTable，按 ucType 选 VAR_*_END_ADDR 上界断言
    for (uint16_t i = 0u; i < tVariableApiTableCount; ++i)
    {
        const ST_DC_VARIABLE_TABLE *row = &tVariableApiTable[i];
        uint16_t limit = 0u;

        switch (row->ucType)
        {
        case VARIABLE_TYPEA:
            limit = VAR_A_END_ADDR;
            break;
        case VARIABLE_TYPEB:
            limit = VAR_B_END_ADDR;
            break;
        case VARIABLE_TYPEC:
            limit = VAR_C_END_ADDR;
            break;
        case VARIABLE_TYPED:
            limit = VAR_D_END_ADDR;
            break;
        default:
            FAIL() << "unknown ucType at row " << i;
        }
        EXPECT_LE(static_cast<uint32_t>(row->eVariableAddr) + static_cast<uint32_t>(row->ucLength),
                  static_cast<uint32_t>(limit))
            << "row " << i;
    }
}

// 测试内容：表中 A→B→C→D 分段且 ucType 单调（8.2.1）
TEST_F(VariableTestBase, ApiTableTypePartition)
{
    const uint8_t expected_order[] = {VARIABLE_TYPEA, VARIABLE_TYPEB, VARIABLE_TYPEC, VARIABLE_TYPED};
    uint8_t segment = 0u;
    uint8_t prev_type = tVariableApiTable[0].ucType;

    // 1. 遍历 tVariableApiTable，记录 ucType 变化
    ASSERT_EQ(prev_type, expected_order[0]);
    for (uint16_t i = 1u; i < tVariableApiTableCount; ++i)
    {
        const uint8_t cur = tVariableApiTable[i].ucType;
        if (cur != prev_type)
        {
            ++segment;
            ASSERT_LT(segment, 4u);
            EXPECT_EQ(cur, expected_order[segment]);
            prev_type = cur;
        }
        EXPECT_EQ(cur, expected_order[segment]) << "row " << i;
    }

    // 2. 断言顺序为 TYPEA、TYPEB、TYPEC、TYPED 各一段
    EXPECT_EQ(segment, 3u);
}

// 测试内容：EE 区 A→B→D 首尾相接（8.2.1）
TEST_F(VariableTestBase, EeMapContiguous)
{
    // 1. 断言 VAR_B_BASE == VAR_A_BASE + VAR_A_EE_TOTAL
    EXPECT_EQ(VAR_B_EEPROM_BASE, VAR_A_EEPROM_BASE + static_cast<uint32_t>(VAR_A_EE_TOTAL));

    // 2. 断言 VAR_D_BASE == VAR_B_BASE + VAR_B_EE_TOTAL
    EXPECT_EQ(VAR_D_EEPROM_BASE,
              VAR_B_EEPROM_BASE + static_cast<uint32_t>(VAR_B_EE_TOTAL));

    // 3. 断言 VAR_EE_TOTAL == VAR_A_EE_TOTAL + VAR_B_EE_TOTAL + VAR_D_EE_SIZE
    EXPECT_EQ(VAR_EE_TOTAL,
              static_cast<uint16_t>(VAR_A_EE_TOTAL + VAR_B_EE_TOTAL + VAR_D_EE_SIZE));
}
