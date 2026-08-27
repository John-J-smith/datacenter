#ifndef DC_VARIABLE_H
#define DC_VARIABLE_H

#include "dc_types.h"
#include "dc_storage_cfg.h"
#include "dc_variable_cfg.h"
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
    VAR_EE_SLOT_A_PWR_ON_1 = 1,
    VAR_EE_SLOT_A_PWR_DWN = 2,
    VAR_EE_SLOT_B_PWR_ON_0 = 3,
    VAR_EE_SLOT_B_PWR_ON_1 = 4,
    VAR_EE_SLOT_B_PWR_DWN = 5,
    VAR_EE_SLOT_D_DATA = 6
} E_VARIABLE_EE_SLOT;

#define VAR_ENUM_ROW(tok, id, n, b) tok = (id),

typedef enum {
    VAR_LIST_A(VAR_ENUM_ROW)
    VAR_LIST_B(VAR_ENUM_ROW)
    VAR_LIST_C(VAR_ENUM_ROW)
    VAR_LIST_D(VAR_ENUM_ROW)
    VARIABLE_ID_SENTINEL = 0
} E_VARIABLE_ID;

#undef VAR_ENUM_ROW

typedef struct {
    uint16_t eVariableType;
    uint16_t eVariableAddr;
    uint16_t ucLenth;
    uint8_t  ucIndexNum;
    uint8_t  ucBytes;
    uint8_t  ucType;
} STR_VARIABLE_API_TABLE;

extern const STR_VARIABLE_API_TABLE tVariableApiTable[];
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
extern const uint16_t VAR_A_EE_PWR_ON_0;
extern const uint16_t VAR_A_EE_PWR_ON_1;
extern const uint16_t VAR_A_EE_PWR_DWN;
extern const uint16_t VAR_A_EE_TOTAL;

extern const uint16_t VAR_B_EE_BANK_SIZE;
extern const uint16_t VAR_B_EE_PWR_ON_0;
extern const uint16_t VAR_B_EE_PWR_ON_1;
extern const uint16_t VAR_B_EE_PWR_DWN;
extern const uint16_t VAR_B_EE_TOTAL;

extern const uint16_t VAR_D_EE_SIZE;
extern const uint16_t VAR_EE_TOTAL;

uint32_t VariableEeSlotAddr(E_VARIABLE_EE_SLOT slot);
int16_t VariableEeReadSlot(E_VARIABLE_EE_SLOT slot, uint8_t *buf, uint16_t len);
int16_t VariableEeWriteSlot(E_VARIABLE_EE_SLOT slot, const uint8_t *buf, uint16_t len);

#endif
