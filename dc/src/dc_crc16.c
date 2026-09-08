#include "dc_crc16.h"

/**
 * @brief CRC16-CCITT（初值 0xFFFF，多项式 0x1021）
 *
 * @param pucBuf 数据；可为 NULL（返回初值）
 * @param usLen  字节数
 * @return CRC 值
 */
uint16_t dc_crc16_ccitt(const uint8_t *pucBuf, uint16_t usLen)
{
    uint16_t usCrc = 0xFFFFu;
    if (pucBuf == 0)
    {
        return usCrc;
    }
    /* 逐字节左移异或，再按位折叠 */
    for (uint16_t i = 0u; i < usLen; i++)
    {
        usCrc ^= (uint16_t)((uint16_t)pucBuf[i] << 8);
        for (uint8_t k = 0u; k < 8u; k++)
        {
            if ((usCrc & 0x8000u) != 0u)
            {
                usCrc = (uint16_t)((usCrc << 1) ^ 0x1021u);
            }
            else
            {
                usCrc = (uint16_t)(usCrc << 1);
            }
        }
    }
    return usCrc;
}
