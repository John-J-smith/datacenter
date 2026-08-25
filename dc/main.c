/**
 * @file main.c
 * @brief dc_meter_fw 可运行示例：别名读写 + 变量参数表查询
 *
 * 变量类、参变量类走 RAM 镜像读写；其它大类入口仍返回 DC_RET_UNSUPPORTED。
 */
#include "datacenter.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const char *dc_ret_name(int16_t ret)
{
    if (ret == DC_RET_ALIAS_ERR) {
        return "DC_RET_ALIAS_ERR";
    }
    if (ret == DC_RET_UNSUPPORTED) {
        return "DC_RET_UNSUPPORTED";
    }
    if (ret == DC_RET_PARAM_ERR) {
        return "DC_RET_PARAM_ERR";
    }
    if (ret >= 0) {
        return "OK(bytes)";
    }
    return "unknown";
}

static const STR_VARIABLE_API_TABLE *find_var(uint16_t subclass)
{
    uint16_t i;

    for (i = 0u; i < tVariableApiTableCount; i++) {
        if (tVariableApiTable[i].eVariableType == subclass) {
            return &tVariableApiTable[i];
        }
    }
    return 0;
}

int main(void)
{
    uint8_t buf[16];
    int16_t ret;
    uint32_t genre;
    const STR_VARIABLE_API_TABLE *row;

    DcDumpLayout();
    printf("\n");

    genre = VarAliasBuild(VARIABLE_RMS_VOLTAGE, 0);
    buf[0] = 0x11u;
    buf[1] = 0x22u;
    buf[2] = 0x33u;
    buf[3] = 0x44u;
    buf[4] = 0x55u;
    buf[5] = 0x66u;
    ret = WriteAliasData(genre, buf, 3u, 0u);
    printf("Write voltage  alias=0x%08lX  usLen=3  -> %d (%s)\n",
           (unsigned long)genre, (int)ret, dc_ret_name(ret));
    buf[0] = 0u;
    buf[1] = 0u;
    buf[2] = 0u;
    buf[3] = 0u;
    buf[4] = 0u;
    buf[5] = 0u;
    ret = ReadAliasData(genre, buf, 3u, 0u);
    printf("Read  voltage  alias=0x%08lX  usLen=3  -> %d (%s)  data=%02X %02X %02X %02X %02X %02X\n",
           (unsigned long)genre, (int)ret, dc_ret_name(ret),
           buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

    genre = VarAliasBuild(VARIABLE_USED_MONTH, 0);
    ret = WriteAliasData(genre, buf, 1u, 0u);
    printf("Write month    alias=0x%08lX  usLen=1  -> %d (%s)\n",
           (unsigned long)genre, (int)ret, dc_ret_name(ret));

    genre = EnergyAliasBuild(0x0001u, 0u);
    ret = WriteAliasData(genre, buf, 1u, 0u);
    printf("Write energy   alias=0x%08lX          -> %d (%s)\n",
           (unsigned long)genre, (int)ret, dc_ret_name(ret));

    ret = ReadAliasData(VarAliasBuild(VARIABLE_DATE_TIME, 0), 0, 1u, 0u);
    printf("Read  datetime NULL+usLen=1           -> %d (%s)\n",
           (int)ret, dc_ret_name(ret));

    row = find_var(VARIABLE_RMS_VOLTAGE);
    if (row == 0) {
        printf("table: VARIABLE_RMS_VOLTAGE not found\n");
        return 1;
    }
    printf("table: voltage id=0x%04X addr=%u len=%u n=%u bytes=%u type=%u\n",
           (unsigned)row->eVariableType,
           (unsigned)row->eVariableAddr,
           (unsigned)row->ucLenth,
           (unsigned)row->ucIndexNum,
           (unsigned)row->ucBytes,
           (unsigned)row->ucType);
    printf("table: %u rows, A crc@%u end@%u, C end@%u\n",
           (unsigned)tVariableApiTableCount,
           (unsigned)VAR_A_CRC_ADDR,
           (unsigned)VAR_A_END_ADDR,
           (unsigned)VAR_C_END_ADDR);

    genre = ParaAliasBuild(PARAM_SEASON_SWTIME, 0);
    ret = ReadAliasData(genre, buf, 1u, 0u);
    printf("Read  season   alias=0x%08lX  usLen=1  -> %d (%s)  last=%02X\n",
           (unsigned long)genre, (int)ret, dc_ret_name(ret), buf[6]);
    buf[6] = 0xAAu;
    ret = WriteAliasData(genre, buf, 1u, 0u);
    printf("Write season   alias=0x%08lX  usLen=1  -> %d (%s)\n",
           (unsigned long)genre, (int)ret, dc_ret_name(ret));
    buf[6] = 0u;
    ret = ReadAliasData(genre, buf, 1u, 0u);
    printf("Read  season   after write             -> %d  last=%02X\n",
           (int)ret, buf[6]);

    genre = ParaAliasBuild(PARAM_HOLIDAY_DATA, 0);
    ret = ReadAliasData(genre, buf, 1u, 0u);
    printf("Read  holiday0 alias=0x%08lX  usLen=1  -> %d (%s)\n",
           (unsigned long)genre, (int)ret, dc_ret_name(ret));

    /* LINKARRAY: PARAM_HOLIDAY_DATA_1 = 20 x 12B, pages block2 (rec 0-9) + block3 (rec 10-19) */
    {
        uint8_t rec12[12];
        uint8_t all240[240];

        genre = ParaAliasBuild(PARAM_HOLIDAY_DATA, 0);
        ret = ReadAliasData(genre, rec12, 1u, 0u);
        printf("Read  hol1[0]  alias=0x%08lX  usLen=1  -> %d (%s)  first=%02X (block2 off0)\n",
               (unsigned long)genre, (int)ret, dc_ret_name(ret), rec12[0]);

        memset(rec12, 0xA5u, sizeof rec12);
        genre = ParaAliasBuild(PARAM_HOLIDAY_DATA, 10);
        ret = WriteAliasData(genre, rec12, 1u, 0u);
        printf("Write hol1[10] alias=0x%08lX  usLen=1  -> %d (%s) (block3 off0)\n",
               (unsigned long)genre, (int)ret, dc_ret_name(ret));

        genre = ParaAliasBuild(PARAM_HOLIDAY_DATA, PARAM_INDEX_ALL);
        ret = ReadAliasData(genre, all240, 20u, 0u);
        printf("Read  hol1[*] alias=0x%08lX  usLen=20 -> %d (%s)  [0]=%02X [120]=%02X\n",
               (unsigned long)genre, (int)ret, dc_ret_name(ret),
               all240[0], all240[10u * 12u]);

        genre = ParaAliasBuild(PARAM_HOLIDAY_DATA, 9);
        ret = ReadAliasData(genre, rec12, 2u, 0u);
        printf("Read  hol1[9,10] alias=0x%08lX usLen=2 -> %d (%s)  page0 tail=%02X page1 head=%02X\n",
               (unsigned long)genre, (int)ret, dc_ret_name(ret), rec12[0], rec12[12]);
    }

    return 0;
}
