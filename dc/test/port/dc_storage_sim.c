#include "datacenter.h"
#include "dc_storage_cfg.h"

#include <string.h>

static uint8_t s_ucStorage[0x2000u];

/**
 * @brief 测试用：存储镜像填 0xFF
 */
void DcTestStorageReset(void)
{
    memset(s_ucStorage, 0xFF, sizeof s_ucStorage);
}

/**
 * @brief 测试用：存储镜像基址
 *
 * @return 模拟存储首地址
 */
uint8_t *DcTestStoragePtr(void)
{
    return s_ucStorage;
}

/**
 * @brief 模拟统一 storage 读
 *
 * @param ulAddr 绝对地址
 * @param pucBuf 输出缓冲
 * @param usLen  字节数
 * @return 成功返回读取字节数；失败返回负错误码
 */
int16_t DcCfgStorageRead(uint32_t ulAddr, uint8_t *pucBuf, uint16_t usLen)
{
    if (ulAddr >= DC_STORAGE_BASE_FILE) {
        return DC_RET_UNSUPPORTED;
    }
    if ((uint32_t)usLen + ulAddr > (uint32_t)(sizeof s_ucStorage)) {
        return DC_RET_PARAM_ERR;
    }
    memcpy(pucBuf, s_ucStorage + ulAddr, (size_t)usLen);
    return (int16_t)usLen;
}

/**
 * @brief 模拟统一 storage 写
 *
 * @param ulAddr 绝对地址
 * @param pucBuf 输入数据
 * @param usLen  字节数
 * @return 成功返回写入字节数；失败返回负错误码
 */
int16_t DcCfgStorageWrite(uint32_t ulAddr, const uint8_t *pucBuf, uint16_t usLen)
{
    if (ulAddr >= DC_STORAGE_BASE_FILE) {
        return DC_RET_UNSUPPORTED;
    }
    if ((uint32_t)usLen + ulAddr > (uint32_t)(sizeof s_ucStorage)) {
        return DC_RET_PARAM_ERR;
    }
    memcpy(s_ucStorage + ulAddr, pucBuf, (size_t)usLen);
    return (int16_t)usLen;
}
