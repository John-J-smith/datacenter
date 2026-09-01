/* Unified non-volatile storage for datacenter modules (EE / Flash / file). */
#ifndef DC_STORAGE_CFG_H
#define DC_STORAGE_CFG_H

#include <stdint.h>

/*
 * Product unified address map (override in project cfg if needed).
 * DcCfgStorageRead/Write receive absolute addresses; product dispatches by range:
 *   [DC_STORAGE_BASE_EE, DC_STORAGE_BASE_FLASH)     -> EEPROM driver
 *   [DC_STORAGE_BASE_FLASH, DC_STORAGE_BASE_FILE)   -> Flash driver
 *   [DC_STORAGE_BASE_FILE, ...)                     -> host file backend (sim/PC)
 *
 * Module cfgs (e.g. VAR_EEPROM_BASE) pick origins inside this space.
 */
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

int16_t DcCfgStorageRead(uint32_t addr, uint8_t *buf, uint16_t len);
int16_t DcCfgStorageWrite(uint32_t addr, const uint8_t *buf, uint16_t len);

#ifndef DC_STORAGE_READ
#define DC_STORAGE_READ(addr, buf, len) DcCfgStorageRead((addr), (buf), (uint16_t)(len))
#endif

#ifndef DC_STORAGE_WRITE
#define DC_STORAGE_WRITE(addr, buf, len) DcCfgStorageWrite((addr), (const uint8_t *)(buf), (uint16_t)(len))
#endif

#endif
