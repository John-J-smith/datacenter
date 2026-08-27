#include "datacenter.h"
#include "dc_storage_cfg.h"

#include <string.h>

static uint8_t s_storage[0x2000u];

void DcTestStorageReset(void)
{
    memset(s_storage, 0xFF, sizeof s_storage);
}

uint8_t *DcTestStoragePtr(void)
{
    return s_storage;
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
