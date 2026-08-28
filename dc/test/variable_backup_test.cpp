#include "variable_test_helpers.hpp"

// 测试内容：A 类定时备份到 PWR_ON 槽（CONTEXT EE 备份）
TEST_F(VariableTestBase, TypeA_PeriodicPwrOnBackup)
{
    uint8_t buf[8];

    // 1. 写 DATE_TIME
    SeedAClassEe(0u);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    buf[0] = 0x31u;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, buf, 1u), 7);

    // 2. var_backup_tick(VAR_A_BACKUP_INTERVAL_SEC)
    var_backup_tick(VAR_A_BACKUP_INTERVAL_SEC);

    // 3. 读 PWR_ON_0/1 首字节断言
    ExpectEeSlotFirstByte(VAR_EE_SLOT_A_PWR_ON_0, 0x31u, VAR_A_END_ADDR);
#if (VAR_EE_BACKUP_BANKS >= 2)
    ExpectEeSlotFirstByte(VAR_EE_SLOT_A_PWR_ON_1, 0x31u, VAR_A_END_ADDR);
#endif
}

// 测试内容：B 类脏标记门控定时备份（CONTEXT B 类备份）
TEST_F(VariableTestBase, TypeB_DirtyGatedBackup)
{
    uint8_t buf[8];
    uint8_t ee[32];

    SeedAClassEe(0u);
    ReadVar(VAR_DATE_TIME, 0, buf, 1u);

    // 1. 未写 B 时 tick 满间隔 → B EE 仍为 0xFF
    var_backup_tick(VAR_B_BACKUP_INTERVAL_SEC);
    VariableEeReadSlot(VAR_EE_SLOT_B_PWR_ON_0, ee, VAR_B_END_ADDR);
    EXPECT_EQ(ee[0], 0xFFu);
#if (VAR_EE_BACKUP_BANKS >= 2)
    VariableEeReadSlot(VAR_EE_SLOT_B_PWR_ON_1, ee, VAR_B_END_ADDR);
    EXPECT_EQ(ee[0], 0xFFu);
#endif

    // 2. 写 B 后 tick 未达间隔 → 不备份
    buf[0] = 0x42u;
    buf[1] = 0u;
    buf[2] = 0u;
    buf[3] = 0u;
    ASSERT_EQ(WriteVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    DcTestVarResetBackupTimers();
    var_backup_tick(VAR_B_BACKUP_INTERVAL_SEC - 1u);
    VariableEeReadSlot(VAR_EE_SLOT_B_PWR_ON_0, ee, VAR_B_END_ADDR);
    EXPECT_EQ(ee[0], 0xFFu);

    // 3. tick 满间隔 → B PWR_ON_0 / PWR_ON_1 更新
    var_backup_tick(1);
    ExpectEeSlotFirstByte(VAR_EE_SLOT_B_PWR_ON_0, 0x42u, VAR_B_END_ADDR);
#if (VAR_EE_BACKUP_BANKS >= 2)
    ExpectEeSlotFirstByte(VAR_EE_SLOT_B_PWR_ON_1, 0x42u, VAR_B_END_ADDR);
#endif
}

// 测试内容：掉电间隔 tick 写入 PWR_DWN 槽（CONTEXT 掉电备份）
TEST_F(VariableTestBase, PwrDwnIntervalBackup)
{
    uint8_t a_buf[8];
    uint8_t b_buf[8];

    // 1. 写 A/B 类变量
    SeedAClassEe(0u);
    ReadVar(VAR_DATE_TIME, 0, a_buf, 1u);
    a_buf[0] = 0x61u;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, a_buf, 1u), 7);
    b_buf[0] = 0x62u;
    b_buf[1] = 0u;
    b_buf[2] = 0u;
    b_buf[3] = 0u;
    ASSERT_EQ(WriteVar(VAR_USED_MONTH, 0, b_buf, 1u), 4);

    // 2. var_backup_tick(VAR_PWR_DWN_INTERVAL_SEC)
    var_backup_tick(VAR_PWR_DWN_INTERVAL_SEC);

    // 3. 断言 A/B 掉电槽首字节
    ExpectEeSlotFirstByte(VAR_EE_SLOT_A_PWR_DWN, 0x61u, VAR_A_END_ADDR);
    ExpectEeSlotFirstByte(VAR_EE_SLOT_B_PWR_DWN, 0x62u, VAR_B_END_ADDR);
}

// 测试内容：var_backup_power_down 立即写掉电槽
TEST_F(VariableTestBase, ImmediatePowerDown)
{
    uint8_t buf[8];

    // 1. 写 A 类变量
    SeedAClassEe(0u);
    ReadVar(VAR_DATE_TIME, 0, buf, 1u);
    buf[0] = 0x71u;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, buf, 1u), 7);

    // 2. 调用 var_backup_power_down()
    var_backup_power_down();

    // 3. 断言 A 掉电槽首字节
    ExpectEeSlotFirstByte(VAR_EE_SLOT_A_PWR_DWN, 0x71u, VAR_A_END_ADDR);
}

// 测试内容：magic 与 CRC 均坏时跳过备份（CONTEXT 备份允许条件）
TEST_F(VariableTestBase, BackupSkippedWhenInvalid)
{
    uint8_t buf[8];

    // 1. Seed EE=0x5A，读入 SRAM 后改写为 0xBB（使 SRAM 与 EE 可区分）
    SeedAClassEe(0x5Au);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    buf[0] = 0xBBu;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, buf, 1u), 7);

    // 2. InvalidateAll(A)，tick 满 A 间隔
    DcTestVarInvalidateAll(DC_TEST_VAR_ZONE_A);
    var_backup_tick(VAR_A_BACKUP_INTERVAL_SEC);

    // 3. 断言 PWR_ON_0 仍为 seed 值 0x5A，而非 SRAM 中的 0xBB
    ExpectEeSlotFirstByte(VAR_EE_SLOT_A_PWR_ON_0, 0x5Au, VAR_A_END_ADDR);
}
