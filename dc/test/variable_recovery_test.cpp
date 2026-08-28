#include "variable_test_helpers.hpp"

// 测试内容：上电恢复优先读掉电区 PWR_DWN（CONTEXT A 类上电链）
TEST_F(VariableTestBase, PwrUpPrefersPwrDwn)
{
    uint8_t buf[8];

    // 1. Seed PWR_ON_0=0x11、PWR_DWN=0x99
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0x11u);
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_DWN, 0x99u);

    // 2. 首次读 DATE_TIME，断言首字节为 0x99
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x99u);
}

// 测试内容：掉电区 CRC 坏时回退 PWR_ON_0（CONTEXT 上电链）
TEST_F(VariableTestBase, PwrUpFallbackPwrOn0)
{
    uint8_t buf[8];

    // 1. Seed PWR_ON_0=0xAA、PWR_DWN 后破坏 CRC
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0xAAu);
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_DWN, 0xBBu);
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_DWN) + VAR_A_CRC_ADDR] = 0u;

    // 2. 首次读 DATE_TIME，断言首字节为 0xAA
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0xAAu);
}

#if (VAR_EE_BACKUP_BANKS >= 2)
// 测试内容：掉电区与 PWR_ON_0 均坏时回退 PWR_ON_1（CONTEXT 上电链）
TEST_F(VariableTestBase, PwrUpFallbackPwrOn1)
{
    uint8_t buf[8];

    // 1. Seed PWR_ON_1=0xCC，PWR_ON_0/PWR_DWN CRC 写坏
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0xDDu);
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_1, 0xCCu);
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_DWN, 0xBBu);
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_0) + VAR_A_CRC_ADDR] = 0u;
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_DWN) + VAR_A_CRC_ADDR] = 0u;

    // 2. 首次读 DATE_TIME，断言首字节为 0xCC
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0xCCu);
}
#endif

// 测试内容：运行中 magic 坏、CRC 好 → 仅补 magic，数据可读（CONTEXT 运行中链）
TEST_F(VariableTestBase, RuntimeMagicBadCrcOk)
{
    uint8_t buf[8];

    // 1. 正常读建立 SRAM
    SeedAClassEe(0x55u);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);

    // 2. CorruptMagic(A)
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_A);

    // 3. 再读 DATE_TIME，断言数据不变
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x55u);

    // 4. 写入新值后 CRC 刷新；magic 再坏时仍读 SRAM 而非 EE
    buf[0] = 0x66u;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    ExpectZoneBodyCrcOk(DC_TEST_VAR_ZONE_A);
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_A);
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x66u);
}

// 测试内容：运行中 magic/CRC 坏 → 从 EE PWR_ON_0 恢复（CONTEXT 运行中链）
TEST_F(VariableTestBase, RuntimeCrcBadRestoreFromPwrOn0)
{
    uint8_t buf[8];

    // 1. Seed EE=0x77，读并写 RAM=0x88
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0x77u);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    buf[0] = 0x88u;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, buf, 1u), 7);

    // 2. CorruptMagic(A) 且 CorruptCrc(A)（magic 错时才查 CRC）
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_A);
    DcTestVarCorruptCrc(DC_TEST_VAR_ZONE_A);

    // 3. 再读，断言恢复为 0x77
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x77u);
}

#if (VAR_EE_BACKUP_BANKS >= 2)
// 测试内容：运行中 PWR_ON_0 坏时回退 PWR_ON_1（CONTEXT 运行中链）
TEST_F(VariableTestBase, RuntimeCrcBadFallbackPwrOn1)
{
    uint8_t buf[8];

    // 1. Seed PWR_ON_0/PWR_ON_1，写 RAM 后 CorruptMagic+CorruptCrc，破坏 PWR_ON_0 CRC
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0x11u);
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_1, 0x22u);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    buf[0] = 0x88u;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_0) + VAR_A_CRC_ADDR] = 0u;
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_A);
    DcTestVarCorruptCrc(DC_TEST_VAR_ZONE_A);

    // 2. 再读，断言来自 PWR_ON_1
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x22u);
}
#endif

// 测试内容：SRAM 与 EE 均不可恢复 → DC_RET_PARAM_ERR（CONTEXT 失败路径）
TEST_F(VariableTestBase, RuntimeRestoreFails)
{
    uint8_t buf[8];

    // 1. 正常读完成 init
    SeedAClassEe(0x11u);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);

    // 2. InvalidateAll(A)，破坏全部 A 区 EE 槽 CRC
    DcTestVarInvalidateAll(DC_TEST_VAR_ZONE_A);
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_0) + VAR_A_CRC_ADDR] = 0u;
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_DWN) + VAR_A_CRC_ADDR] = 0u;
#if (VAR_EE_BACKUP_BANKS >= 2)
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_1) + VAR_A_CRC_ADDR] = 0u;
#endif

    // 3. 读 DATE_TIME，断言 DC_RET_PARAM_ERR
    EXPECT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), DC_RET_PARAM_ERR);
}

// 测试内容：B 区运行中 magic 坏、CRC 好 → 仅补 magic
TEST_F(VariableTestBase, TypeB_RuntimeMagicBadCrcOk)
{
    uint8_t buf[8];

    // 1. Seed B EE 并读入 SRAM（body 含合法 CRC）
    SeedAClassEe(0u);
    SeedBClassEeSlot(VAR_EE_SLOT_B_PWR_ON_0, 0x33u);
    ReadVar(VAR_DATE_TIME, 0, buf, 1u);
    ASSERT_EQ(ReadVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    EXPECT_EQ(buf[0], 0x33u);

    // 2. CorruptMagic(B)
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_B);

    // 3. 再读 USED_MONTH 断言不变
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    EXPECT_EQ(buf[0], 0x33u);

    // 4. 写入新值后 CRC 刷新；magic 再坏时仍读 SRAM
    buf[0] = 0x44u;
    ASSERT_EQ(WriteVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    ExpectZoneBodyCrcOk(DC_TEST_VAR_ZONE_B);
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_B);
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    EXPECT_EQ(buf[0], 0x44u);
}

// 测试内容：B 区运行中 magic/CRC 坏 → 从 EE 恢复
TEST_F(VariableTestBase, TypeB_RuntimeCrcBadRestore)
{
    uint8_t buf[8];

    // 1. Seed B EE=0x44 并读入 SRAM
    SeedAClassEe(0u);
    SeedBClassEeSlot(VAR_EE_SLOT_B_PWR_ON_0, 0x44u);
    ReadVar(VAR_DATE_TIME, 0, buf, 1u);
    ASSERT_EQ(ReadVar(VAR_USED_MONTH, 0, buf, 1u), 4);

    // 2. 写 RAM=0x99 后 CorruptMagic+CorruptCrc
    buf[0] = 0x99u;
    ASSERT_EQ(WriteVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_B);
    DcTestVarCorruptCrc(DC_TEST_VAR_ZONE_B);

    // 3. 再读 USED_MONTH，断言 EE 值 0x44
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    EXPECT_EQ(buf[0], 0x44u);
}

// 测试内容：B 区不可恢复 → DC_RET_PARAM_ERR
TEST_F(VariableTestBase, TypeB_RuntimeRestoreFails)
{
    uint8_t buf[8] = {0x01u, 0x00u, 0x00u, 0x00u};

    // 1. 读 B 类完成 init
    SeedAClassEe(0u);
    ReadVar(VAR_DATE_TIME, 0, buf, 1u);
    ReadVar(VAR_USED_MONTH, 0, buf, 1u);

    // 2. InvalidateAll(B)，破坏 B 区 EE 槽 CRC
    DcTestVarInvalidateAll(DC_TEST_VAR_ZONE_B);
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_B_PWR_ON_0) + VAR_B_CRC_ADDR] = 0u;
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_B_PWR_DWN) + VAR_B_CRC_ADDR] = 0u;
#if (VAR_EE_BACKUP_BANKS >= 2)
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_B_PWR_ON_1) + VAR_B_CRC_ADDR] = 0u;
#endif

    // 3. 读 USED_MONTH，断言 DC_RET_PARAM_ERR
    EXPECT_EQ(ReadVar(VAR_USED_MONTH, 0, buf, 1u), DC_RET_PARAM_ERR);
}
