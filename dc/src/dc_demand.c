#include "dc_entry.h"
#include "datacenter.h"

/**
 * @brief 读需量（尚未实现）
 *
 * @param ulAlias 需量别名
 * @param pucBuf  输出缓冲
 * @param usLen   元素个数
 * @param ucType  保留，传 0
 * @return DC_RET_UNSUPPORTED
 */
int16_t dc_read_demand(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType)
{
    (void)ulAlias;
    (void)pucBuf;
    (void)usLen;
    (void)ucType;
    return DC_RET_UNSUPPORTED;
}

/**
 * @brief 写需量（尚未实现）
 *
 * @param ulAlias 需量别名
 * @param pucBuf  输入数据
 * @param usLen   元素个数
 * @param ucType  保留，传 0
 * @return DC_RET_UNSUPPORTED
 */
int16_t dc_write_demand(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType)
{
    (void)ulAlias;
    (void)pucBuf;
    (void)usLen;
    (void)ucType;
    return DC_RET_UNSUPPORTED;
}
