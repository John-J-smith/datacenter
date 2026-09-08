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

#ifndef PARAM_EEPROM_BASE
#define PARAM_EEPROM_BASE (VAR_EEPROM_BASE + 512u)
#endif

/*
 * Product firmware: define DC_NOINIT to a no-init qualifier (e.g. IAR __no_init)
 * so A/B variable RAM and param SRAM blocks survive reset. Host tests leave it empty.
 */
#ifndef DC_NOINIT
#define DC_NOINIT
#endif

int16_t DcCfgStorageRead(uint32_t ulAddr, uint8_t *pucBuf, uint16_t usLen);
int16_t DcCfgStorageWrite(uint32_t ulAddr, const uint8_t *pucBuf, uint16_t usLen);

#ifndef DC_STORAGE_READ
#define DC_STORAGE_READ(ulAddr, pucBuf, usLen) DcCfgStorageRead((ulAddr), (pucBuf), (uint16_t)(usLen))
#endif

#ifndef DC_STORAGE_WRITE
#define DC_STORAGE_WRITE(ulAddr, pucBuf, usLen) DcCfgStorageWrite((ulAddr), (const uint8_t *)(pucBuf), (uint16_t)(usLen))
#endif

#endif
