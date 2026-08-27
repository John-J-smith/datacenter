#include "datacenter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_ee[0x2000u];

int16_t DcCfgEeRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    if ((uint32_t)len + addr > (uint32_t)(sizeof s_ee)) {
        return DC_RET_PARAM_ERR;
    }
    memcpy(buf, s_ee + addr, (size_t)len);
    return (int16_t)len;
}

int16_t DcCfgEeWrite(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    if ((uint32_t)len + addr > (uint32_t)(sizeof s_ee)) {
        return DC_RET_PARAM_ERR;
    }
    memcpy(s_ee + addr, buf, (size_t)len);
    return (int16_t)len;
}

int main(void)
{
    uint8_t buf[32];
    int16_t ret;
    uint32_t g;

    memset(s_ee, 0xFF, sizeof s_ee);

    assert(VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_ON_0) == (uint32_t)VAR_EEPROM_BASE);
    assert(VAR_A_EEPROM_BASE == (uint32_t)VAR_EEPROM_BASE);
    assert(VAR_B_EEPROM_BASE == (uint32_t)VAR_EEPROM_BASE + (uint32_t)VAR_A_EE_TOTAL);
    assert(VAR_D_EEPROM_BASE ==
           (uint32_t)VAR_EEPROM_BASE + (uint32_t)VAR_A_EE_TOTAL + (uint32_t)VAR_B_EE_TOTAL);
    assert(VariableEeSlotAddr(VAR_EE_SLOT_D_DATA) == VAR_D_EEPROM_BASE);

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
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 7);
    buf[0] = 0x26u;
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 7);

    g = VarAliasBuild(VARIABLE_MTWORK_EVTKEY, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 8);
    memset(buf, 0xA5u, 8u);
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 8);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 8);
    assert(buf[0] == 0xA5u);
    assert(s_ee[VAR_D_EEPROM_BASE] == 0xA5u);

    printf("variable_rw ok\n");
    return 0;
}
