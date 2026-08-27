#ifndef DATACENTER_H
#define DATACENTER_H

#include "dc_alias.h"
#include "dc_variable.h"
#include "dc_param.h"
#include <stdint.h>

#define DC_RET_ALIAS_ERR     ((int16_t)-1)
#define DC_RET_UNSUPPORTED   ((int16_t)-2)
#define DC_RET_PARAM_ERR     ((int16_t)-3)

int16_t ReadAliasData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t WriteAliasData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

void VariableBackupTick(uint16_t elapsed_sec);
void VariableBackupPowerDown(void);

#endif
