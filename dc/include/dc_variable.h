#ifndef DC_VARIABLE_H
#define DC_VARIABLE_H

#include "dc_storage_cfg.h"
#include "dc_variable_cfg.h"
#include "dc_variable_layout.h"
#include <stdint.h>

#define VAR_CRC_BYTES_BLOCK (2u)
#define VAR_SRAM_MAGIC_HEAD (0xA5A5A5A5u)
#define VAR_SRAM_MAGIC_TAIL (0x5A5A5A5Au)

typedef enum {
    VARIABLE_TYPEA = 0, /* SRAM + EE 定时/掉电备份 */
    VARIABLE_TYPEB = 1, /* SRAM + EE 变化触发/掉电备份 */
    VARIABLE_TYPEC = 2, /* 仅 SRAM */
    VARIABLE_TYPED = 3  /* 仅 EE，无校验备份 */
} E_VARIABLE_STOR_TYPE;

typedef enum {
    VAR_EE_SLOT_A_PWR_ON_0 = 0,
#if (VAR_EE_BACKUP_BANKS >= 2)
    VAR_EE_SLOT_A_PWR_ON_1,
#endif
    VAR_EE_SLOT_A_PWR_DWN,
    VAR_EE_SLOT_B_PWR_ON_0,
#if (VAR_EE_BACKUP_BANKS >= 2)
    VAR_EE_SLOT_B_PWR_ON_1,
#endif
    VAR_EE_SLOT_B_PWR_DWN,
    VAR_EE_SLOT_D_DATA,
    VAR_EE_SLOT_COUNT
} E_VARIABLE_EE_SLOT;

typedef struct {
    uint16_t eVariableType;
    uint16_t eVariableAddr;
    uint16_t ucLength;
    uint8_t  ucIndexNum;
    uint8_t  ucBytes;
    uint8_t  ucType;
} ST_DC_VARIABLE_TABLE;

extern const ST_DC_VARIABLE_TABLE tVariableApiTable[];
extern const uint16_t tVariableApiTableCount;

extern const uint16_t VAR_A_CRC_ADDR;
extern const uint16_t VAR_A_END_ADDR;
extern const uint16_t VAR_B_CRC_ADDR;
extern const uint16_t VAR_B_END_ADDR;
extern const uint16_t VAR_C_END_ADDR;
extern const uint16_t VAR_D_END_ADDR;

extern const uint32_t VAR_A_EEPROM_BASE;
extern const uint32_t VAR_B_EEPROM_BASE;
extern const uint32_t VAR_D_EEPROM_BASE;

extern const uint16_t VAR_A_EE_BANK_SIZE;
extern const uint16_t VAR_A_EE_PWR_ON_COUNT;
extern const uint16_t VAR_A_EE_PWR_ON_0;
#if (VAR_EE_BACKUP_BANKS >= 2)
extern const uint16_t VAR_A_EE_PWR_ON_1;
#endif
extern const uint16_t VAR_A_EE_PWR_DWN;
extern const uint16_t VAR_A_EE_TOTAL;

extern const uint16_t VAR_B_EE_BANK_SIZE;
extern const uint16_t VAR_B_EE_PWR_ON_COUNT;
extern const uint16_t VAR_B_EE_PWR_ON_0;
#if (VAR_EE_BACKUP_BANKS >= 2)
extern const uint16_t VAR_B_EE_PWR_ON_1;
#endif
extern const uint16_t VAR_B_EE_PWR_DWN;
extern const uint16_t VAR_B_EE_TOTAL;

extern const uint16_t VAR_D_EE_SIZE;
extern const uint16_t VAR_EE_TOTAL;

uint32_t VariableEeSlotAddr(E_VARIABLE_EE_SLOT slot);
int16_t VariableEeReadSlot(E_VARIABLE_EE_SLOT slot, uint8_t *buf, uint16_t len);
int16_t VariableEeWriteSlot(E_VARIABLE_EE_SLOT slot, const uint8_t *buf, uint16_t len);

void var_backup_tick(uint16_t elapsed_sec);
void var_backup_power_down(void);

#endif
