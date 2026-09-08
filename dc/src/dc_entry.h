#ifndef DC_ENTRY_H
#define DC_ENTRY_H

#include <stdint.h>

int16_t dc_read_energy(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);

int16_t dc_read_demand(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);
int16_t dc_write_demand(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);

int16_t dc_read_param(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);
int16_t dc_write_param(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);

int16_t dc_read_variable(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);
int16_t dc_write_variable(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);

int16_t dc_read_list(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);
int16_t dc_write_list(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);

int16_t dc_read_record(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);
int16_t dc_write_record(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType);

#endif
