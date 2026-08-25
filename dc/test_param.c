#include "datacenter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint8_t buf[16];
    uint8_t all[256];
    int16_t ret;
    uint32_t g;

    g = ParaAliasBuild(PARAM_SEASON_SWTIME, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 7);
    assert(buf[6] == 0xFFu);

    buf[0] = 0x26u;
    buf[1] = 0x08u;
    buf[2] = 0x21u;
    buf[3] = 0x00u;
    buf[4] = 0x00u;
    buf[5] = 0x00u;
    buf[6] = 0xAAu;
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 7);
    memset(buf, 0, sizeof buf);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 7);
    assert(buf[6] == 0xAAu);

    g = ParaAliasBuild(PARAM_HOLIDAY_DATA, 1);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 12);
    assert(buf[0] == 0xFFu);

    buf[0] = 0x25u;
    buf[1] = 0x01u;
    buf[2] = 0x01u;
    buf[3] = 0x00u;
    buf[4] = 0x00u;
    buf[5] = 0x01u;
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 12);

    g = ParaAliasBuild(PARAM_HOLIDAY_DATA, PARAM_INDEX_ALL);
    ret = ReadAliasData(g, all, 20u, 0u);
    assert(ret == 240);
    assert(all[12] == 0x25u);

    g = ParaAliasBuild(PARAM_HOLIDAY_DATA, 19);
    ret = ReadAliasData(g, buf, 2u, 0u);
    assert(ret == DC_RET_PARAM_ERR);

    g = ParaAliasBuild(0x40FFu, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == DC_RET_ALIAS_ERR);

    g = ParaAliasBuild(PARAM_CALIB_DATA, 0);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 12);
    assert(buf[0] == 0xFFu);

    memset(buf, 0xA5u, 12u);
    g = ParaAliasBuild(PARAM_CALIB_DATA, 10);
    ret = WriteAliasData(g, buf, 1u, 0u);
    assert(ret == 12);
    memset(buf, 0, sizeof buf);
    ret = ReadAliasData(g, buf, 1u, 0u);
    assert(ret == 12);
    assert(buf[0] == 0xA5u);

    g = ParaAliasBuild(PARAM_CALIB_DATA, PARAM_INDEX_ALL);
    ret = ReadAliasData(g, all, 20u, 0u);
    assert(ret == 240);
    assert(all[10u * 12u] == 0xA5u);
    assert(all[0] == 0xFFu);

    g = ParaAliasBuild(PARAM_CALIB_DATA, 19);
    ret = ReadAliasData(g, buf, 2u, 0u);
    assert(ret == DC_RET_PARAM_ERR);

    printf("param_rw ok\n");
    return 0;
}
