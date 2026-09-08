#include "dc_entry.h"
#include "datacenter.h"

/**
 * @brief 读电量（尚未实现）
 *
 * @param ulAlias 电量别名
 * @param pucBuf  输出缓冲
 * @param usLen   元素个数
 * @param ucType  保留，传 0
 * @return DC_RET_UNSUPPORTED
 */
int16_t dc_read_energy(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType)
{
    (void)ulAlias;
    (void)pucBuf;
    (void)usLen;
    (void)ucType;
    return DC_RET_UNSUPPORTED;
}
