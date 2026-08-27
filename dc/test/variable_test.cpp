#include <gtest/gtest.h>

extern "C" {
#include "datacenter.h"
#include "dc_test_storage.h"
}

namespace {

uint16_t Crc16Ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;

    for (uint16_t i = 0u; i < len; i++) {
        crc ^= static_cast<uint16_t>(static_cast<uint16_t>(data[i]) << 8);
        for (uint8_t b = 0u; b < 8u; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021u);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

void SeedAClassEe(uint8_t first_byte)
{
    uint8_t block[64];
    uint16_t crc;

    memset(block, 0, sizeof block);
    block[0] = first_byte;
    crc = Crc16Ccitt(block, VAR_A_CRC_ADDR);
    block[VAR_A_CRC_ADDR] = static_cast<uint8_t>(crc & 0xFFu);
    block[VAR_A_CRC_ADDR + 1u] = static_cast<uint8_t>(crc >> 8);
    ASSERT_EQ(DcCfgStorageWrite(static_cast<uint32_t>(VAR_EEPROM_BASE), block, VAR_A_END_ADDR),
              static_cast<int16_t>(VAR_A_END_ADDR));
}

class VariableTest : public ::testing::Test {
protected:
    void SetUp() override { DcTestStorageReset(); }
};

TEST_F(VariableTest, EeLayout)
{
    SeedAClassEe(0x99u);

    EXPECT_EQ(VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_0), static_cast<uint32_t>(VAR_EEPROM_BASE));
    EXPECT_EQ(VAR_A_EEPROM_BASE, static_cast<uint32_t>(VAR_EEPROM_BASE));
    EXPECT_EQ(VAR_B_EEPROM_BASE, static_cast<uint32_t>(VAR_EEPROM_BASE) + static_cast<uint32_t>(VAR_A_EE_TOTAL));
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

TEST_F(VariableTest, ReadWrite)
{
    uint8_t buf[32];
    int16_t ret;
    uint32_t g;

    SeedAClassEe(0x99u);

    g = VarAliasBuild(VARIABLE_DATE_TIME, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 7);
    EXPECT_EQ(buf[0], 0x99u);

    g = VarAliasBuild(VARIABLE_RMS_VOLTAGE, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 2);

    buf[0] = 0x22u;
    buf[1] = 0x08u;
    ret = WriteAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 2);
    memset(buf, 0, sizeof buf);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 2);
    EXPECT_EQ(buf[0], 0x22u);

    g = VarAliasBuild(VARIABLE_DATE_TIME, 0);
    buf[0] = 0x26u;
    ret = WriteAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 7);
    VariableBackupTick(VAR_A_BACKUP_INTERVAL_SEC);

    g = VarAliasBuild(VARIABLE_MTWORK_EVTKEY, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 8);
    memset(buf, 0xA5u, 8u);
    ret = WriteAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 8);
    ret = ReadAliasData(g, buf, 1u, 0u);
    ASSERT_EQ(ret, 8);
    EXPECT_EQ(buf[0], 0xA5u);
    EXPECT_EQ(DcTestStoragePtr()[VAR_D_EEPROM_BASE], 0xA5u);
}

}  // namespace
