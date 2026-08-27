#include "datacenter.h"
#include "dc_storage_cfg.h"

int16_t DcCfgEeRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    (void)addr;
    (void)buf;
    (void)len;
    return DC_RET_UNSUPPORTED;
}

int16_t DcCfgEeWrite(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    (void)addr;
    (void)buf;
    (void)len;
    return DC_RET_UNSUPPORTED;
}
