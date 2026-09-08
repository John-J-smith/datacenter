#ifndef DC_CRC16_H
#define DC_CRC16_H

#include <stdint.h>

/**
 * @brief CRC16-CCITT（初值 0xFFFF，多项式 0x1021）
 *
 * @param pucBuf 数据；可为 NULL（返回初值）
 * @param usLen  字节数
 * @return CRC 值
 */
uint16_t dc_crc16_ccitt(const uint8_t *pucBuf, uint16_t usLen);

#endif
