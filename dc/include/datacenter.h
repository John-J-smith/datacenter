#ifndef DATACENTER_H
#define DATACENTER_H

#include "dc_alias.h"
#include "dc_alias_layout.h"
#include "dc_variable.h"
#include "dc_param.h"
#include <stdint.h>

#define DC_RET_ALIAS_ERR     ((int16_t)-1)
#define DC_RET_UNSUPPORTED   ((int16_t)-2)
#define DC_RET_PARAM_ERR     ((int16_t)-3)

int16_t dc_read_alias(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_alias(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

void var_backup_tick(uint16_t elapsed_sec);
void var_backup_power_down(void);

#endif
