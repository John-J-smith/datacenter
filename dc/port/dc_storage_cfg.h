/* Copy to product port/ as dc_storage_cfg.h and customize address bases. */
#ifndef DC_STORAGE_CFG_H
#define DC_STORAGE_CFG_H

#include <stdint.h>

#ifndef DC_STORAGE_BASE_EE
#define DC_STORAGE_BASE_EE    (0x00001000u)
#endif
#ifndef DC_STORAGE_BASE_FLASH
#define DC_STORAGE_BASE_FLASH (0x00100000u)
#endif
#ifndef DC_STORAGE_BASE_FILE
#define DC_STORAGE_BASE_FILE  (0x80000000u)
#endif

#ifndef VAR_EEPROM_BASE
#define VAR_EEPROM_BASE DC_STORAGE_BASE_EE
#endif

int16_t DcCfgStorageRead(uint32_t addr, uint8_t *buf, uint16_t len);
int16_t DcCfgStorageWrite(uint32_t addr, const uint8_t *buf, uint16_t len);

#ifndef DC_STORAGE_READ
#define DC_STORAGE_READ(addr, buf, len) DcCfgStorageRead((addr), (buf), (uint16_t)(len))
#endif

#ifndef DC_STORAGE_WRITE
#define DC_STORAGE_WRITE(addr, buf, len) DcCfgStorageWrite((addr), (const uint8_t *)(buf), (uint16_t)(len))
#endif

#endif
