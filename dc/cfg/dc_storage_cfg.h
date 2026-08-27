/* Generic EEPROM access for datacenter modules. Product layer implements hooks. */
#ifndef DC_STORAGE_CFG_H
#define DC_STORAGE_CFG_H

#include <stdint.h>

int16_t DcCfgEeRead(uint32_t addr, uint8_t *buf, uint16_t len);
int16_t DcCfgEeWrite(uint32_t addr, const uint8_t *buf, uint16_t len);

#ifndef VAR_EE_READ
#define VAR_EE_READ(addr, buf, len) \
    DcCfgEeRead((addr), (buf), (uint16_t)(len))
#endif

#ifndef VAR_EE_WRITE
#define VAR_EE_WRITE(addr, buf, len) \
    DcCfgEeWrite((addr), (const uint8_t *)(buf), (uint16_t)(len))
#endif

#endif
