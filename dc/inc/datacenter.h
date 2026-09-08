#ifndef DATACENTER_H
#define DATACENTER_H

#include "dc_alias.h"
#include "dc_alias_layout.h"
#include <stdint.h>

#define DC_RET_ALIAS_ERR     ((int16_t)-1)
#define DC_RET_UNSUPPORTED   ((int16_t)-2)
#define DC_RET_PARAM_ERR     ((int16_t)-3)

int16_t dc_read_alias(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);
int16_t dc_write_alias(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);

/**
 * @brief 定时备份 A/B 区与掉电区
 *
 * @param usElapsedSec 距上次 tick 经过的秒数
 */
void var_backup_tick(uint16_t usElapsedSec);

/**
 * @brief 立即掉电备份 A/B 区
 */
void var_backup_power_down(void);

#endif
