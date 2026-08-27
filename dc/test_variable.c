#include "datacenter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_storage[0x2000u];

static uint16_t test_crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t b;

    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (b = 0u; b < 8u; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

int16_t DcCfgStorageRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    if (addr >= DC_STORAGE_BASE_FILE) {
        return DC_RET_UNSUPPORTED;
    }
    if ((uint32_t)len + addr > (uint32_t)(sizeof s_storage)) {
        return DC_RET_PARAM_ERR;
    }
    memcpy(buf, s_storage + addr, (size_t)len);
    return (int16_t)len;
}

int16_t DcCfgStorageWrite(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    if (addr >= DC_STORAGE_BASE_FILE) {
        return DC_RET_UNSUPPORTED;
    }
    if ((uint32_t)len + addr > (uint32_t)(sizeof s_storage)) {
        return DC_RET_PARAM_ERR;
    }
    memcpy(s_storage + addr, buf, (size_t)len);
    return (int16_t)len;
}

int main(void)
{
    uint8_t block[64];
    uint8_t buf[32];
    int16_t ret;
    uint32_t g;
    uint16_t crc;

    memset(s_storage, 0xFF, sizeof s_storage);

    /* Seed EE before first access; init restores A body on first read */
    memset(block, 0, sizeof block);
    block[0] = 0x99u;
    crc = test_crc16_ccitt(block, VAR_A_CRC_ADDR);
    block[VAR_A_CRC_ADDR] = (uint8_t)(crc & 0xFFu);
    block[VAR_A_CRC_ADDR + 1u] = (uint8_t)(crc >> 8);
    assert(DcCfgStorageWrite((uint32_t)VAR_EEPROM_BASE, block, VAR_A_END_ADDR) ==
           (int16_t)VAR_A_END_ADDR);

    assert(VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_0) == (uint32_t)VAR_EEPROM_BASE);
    assert(VAR_A_EEPROM_BASE == (uint32_t)VAR_EEPROM_BASE);
    assert(VAR_B_EEPROM_BASE == (uint32_t)VAR_EEPROM_BASE + (uint32_t)VAR_A_EE_TOTAL);
    assert(VAR_D_EEPROM_BASE ==
           (uint32_t)VAR_EEPROM_BASE + (uint32_t)VAR_A_EE_TOTAL + (uint32_t)VAR_B_EE_TOTAL);
    assert(VariableEeSlotAddr(VAR_EE_SLOT_D_DATA) == VAR_D_EEPROM_BASE);
#if (VAR_EE_BACKUP_BANKS >= 2)
    assert(VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_1) ==
           (uint32_t)VAR_EEPROM_BASE + (uint32_t)VAR_A_EE_PWR_ON_1);
    assert(VAR_A_EE_TOTAL == (uint16_t)(3u * VAR_A_END_ADDR));
#else
    assert(VAR_A_EE_TOTAL == (uint16_t)(2u * VAR_A_END_ADDR));
#endif

    g = VarAliasBuild(VARIABLE_DATE_TIME, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 7);
    assert(buf[0] == 0x99u);

    g = VarAliasBuild(VARIABLE_RMS_VOLTAGE, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 2);

    buf[0] = 0x22u;
    buf[1] = 0x08u;
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 2);
    memset(buf, 0, sizeof buf);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 2);
    assert(buf[0] == 0x22u);

    g = VarAliasBuild(VARIABLE_DATE_TIME, 0);
    buf[0] = 0x26u;
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 7);
    VariableBackupTick(VAR_A_BACKUP_INTERVAL_SEC);

    g = VarAliasBuild(VARIABLE_MTWORK_EVTKEY, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 8);
    memset(buf, 0xA5u, 8u);
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 8);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 8);
    assert(buf[0] == 0xA5u);
    assert(s_storage[VAR_D_EEPROM_BASE] == 0xA5u);

    printf("variable_rw ok\n");
    return 0;
}
