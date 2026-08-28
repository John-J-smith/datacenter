#include "variable_test_helpers.hpp"

#include <algorithm>
#include <string>

namespace {

void TraceVarEntry(uint16_t row, const ST_DC_VARIABLE_TABLE *entry, uint8_t index)
{
    SCOPED_TRACE("row=" + std::to_string(row) + " id=0x" +
                 std::to_string(entry->eVariableType) + " index=" +
                 std::to_string(static_cast<unsigned>(index)));
}

}  // namespace

// 测试内容：tVariableApiTable 中全部变量、全部分项 index 读写回读（8.2.2 A/B/C/D）
TEST_F(VariableTestBase, AllVariables_ReadWrite)
{
    std::vector<uint8_t> wbuf(MakeVarIoBuffer());
    std::vector<uint8_t> rbuf(MakeVarIoBuffer());

    // 1. 初始化变量模块（A 类上电恢复链）
    InitVariableModule();

    // 2. 遍历映射表，逐变量、逐 index 写入图案
    for (uint16_t row = 0u; row < tVariableApiTableCount; ++row)
    {
        const ST_DC_VARIABLE_TABLE *entry = &tVariableApiTable[row];
        const uint16_t nbytes = static_cast<uint16_t>(entry->ucBytes);

        for (uint8_t index = 0u; index < entry->ucIndexNum; ++index)
        {
            TraceVarEntry(row, entry, index);
            FillVarWritePattern(wbuf.data(), nbytes, row, index);
            ASSERT_EQ(WriteVar(entry->eVariableType, index, wbuf.data(), 1u),
                      static_cast<int16_t>(nbytes));
        }
    }

    // 3. 全部写完后，再重读一遍并与期望图案比对
    for (uint16_t row = 0u; row < tVariableApiTableCount; ++row)
    {
        const ST_DC_VARIABLE_TABLE *entry = &tVariableApiTable[row];
        const uint16_t nbytes = static_cast<uint16_t>(entry->ucBytes);

        for (uint8_t index = 0u; index < entry->ucIndexNum; ++index)
        {
            TraceVarEntry(row, entry, index);
            FillVarWritePattern(wbuf.data(), nbytes, row, index);
            std::fill(rbuf.begin(), rbuf.end(), 0u);
            ASSERT_EQ(ReadVar(entry->eVariableType, index, rbuf.data(), 1u),
                      static_cast<int16_t>(nbytes));
            EXPECT_EQ(std::memcmp(wbuf.data(), rbuf.data(), nbytes), 0);
        }
    }
}

// 测试内容：A/B 类写 SRAM 后 body CRC 与数据一致（8.2.2）
TEST_F(VariableTestBase, TypeAB_WriteRefreshesSramCrc)
{
    uint8_t buf[8];

    InitVariableModule();

    // 1. 写 A 类变量，断言 SRAM body CRC 合法
    buf[0] = 0xA1u;
    ASSERT_EQ(WriteVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    ExpectZoneBodyCrcOk(DC_TEST_VAR_ZONE_A);

    // 2. magic 坏、CRC 好 → 仍读回刚写入的值
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_A);
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0xA1u);

    // 3. 写 B 类变量，断言 SRAM body CRC 合法
    buf[0] = 0xB2u;
    ASSERT_EQ(WriteVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    ExpectZoneBodyCrcOk(DC_TEST_VAR_ZONE_B);

    // 4. magic 坏、CRC 好 → 仍读回刚写入的值
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_B);
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VAR_USED_MONTH, 0, buf, 1u), 4);
    EXPECT_EQ(buf[0], 0xB2u);
}

// 测试内容：C 类写操作不修改 EE 模拟区（8.2.2 TYPEC 仅 SRAM）
TEST_F(VariableTestBase, TypeC_NoEeSideEffect)
{
    uint8_t buf[4] = {0x22u, 0x08u};
    uint8_t ee_before[16];

    InitVariableModule();

    // 1. 记录 EE 区快照
    std::memcpy(ee_before, DcTestStoragePtr() + VAR_A_EEPROM_BASE, sizeof ee_before);

    // 2. 写 C 类变量
    ASSERT_EQ(WriteVar(VAR_RMS_VOLTAGE, 0, buf, 1u), 2);

    // 3. 断言 EE 快照不变
    EXPECT_EQ(std::memcmp(ee_before, DcTestStoragePtr() + VAR_A_EEPROM_BASE, sizeof ee_before), 0);
}

// 测试内容：无效变量小类返回 DC_RET_ALIAS_ERR
TEST_F(VariableTestBase, InvalidAlias)
{
    uint8_t buf[4];

    // 1. 用无效 type id 调用 dc_read_alias，断言 DC_RET_ALIAS_ERR
    EXPECT_EQ(ReadVar(0xFFFFu, 0, buf, 1u), DC_RET_ALIAS_ERR);
}

// 测试内容：index+usLen 越界返回 DC_RET_PARAM_ERR
TEST_F(VariableTestBase, IndexOutOfRange)
{
    uint8_t buf[8];

    InitVariableModule();

    // 1. RMS_VOLTAGE index=2, usLen=2（越界），断言 DC_RET_PARAM_ERR
    EXPECT_EQ(ReadVar(VAR_RMS_VOLTAGE, 2, buf, 2u), DC_RET_PARAM_ERR);
}

// 测试内容：usLen==0 返回 0
TEST_F(VariableTestBase, ZeroLength)
{
    uint8_t buf[4];

    InitVariableModule();

    // 1. ReadVar usLen=0，断言返回 0
    EXPECT_EQ(ReadVar(VAR_DATE_TIME, 0, buf, 0u), 0);
}

// 测试内容：dataPtr==NULL 且 usLen!=0 返回 DC_RET_PARAM_ERR（alias 层）
TEST_F(VariableTestBase, NullBufferWithLength)
{
    // 1. dc_read_alias(NULL, usLen=1)，断言 DC_RET_PARAM_ERR
    EXPECT_EQ(dc_read_alias(VarAliasBuild(VAR_DATE_TIME, 0), nullptr, 1u, 0u),
              DC_RET_PARAM_ERR);
}

// 测试内容：非法 EE 槽位读写返回 DC_RET_PARAM_ERR
TEST_F(VariableTestBase, InvalidEeSlot)
{
    uint8_t buf[4];

    // 1. VariableEeReadSlot/WriteSlot(VAR_EE_SLOT_COUNT, ...)，断言 DC_RET_PARAM_ERR
    EXPECT_EQ(VariableEeReadSlot(VAR_EE_SLOT_COUNT, buf, 4u), DC_RET_PARAM_ERR);
    EXPECT_EQ(VariableEeWriteSlot(VAR_EE_SLOT_COUNT, buf, 4u), DC_RET_PARAM_ERR);
}
