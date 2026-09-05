#ifndef DC_CRC16_H
#define DC_CRC16_H

#include <stdint.h>

uint16_t dc_crc16_ccitt(const uint8_t *data, uint16_t len);

#endif
