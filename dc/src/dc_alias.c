#include "datacenter.h"
#include "dc_alias.h"
#include "dc_entry.h"

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, uint8_t *, uint16_t, uint8_t);
} ST_ALIAS_RSTORAGE_TABLE;

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, const uint8_t *, uint16_t, uint8_t);
} ST_ALIAS_WSTORAGE_TABLE;

static const ST_ALIAS_RSTORAGE_TABLE s_tReadAliasTable[] = {
    { (uint8_t)ALIAS_CLASS_ENERGY,    dc_read_energy },
    { (uint8_t)ALIAS_CLASS_DEMAND,    dc_read_demand },
    { (uint8_t)ALIAS_CLASS_PARAMETER, dc_read_param },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  dc_read_variable },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, dc_read_list },
    { (uint8_t)ALIAS_CLASS_RECORD,    dc_read_record },
};

static const ST_ALIAS_WSTORAGE_TABLE s_tWriteAliasTable[] = {
    { (uint8_t)ALIAS_CLASS_DEMAND,    dc_write_demand },
    { (uint8_t)ALIAS_CLASS_PARAMETER, dc_write_param },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  dc_write_variable },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, dc_write_list },
    { (uint8_t)ALIAS_CLASS_RECORD,    dc_write_record },
};

/**
 * @brief 按别名大类分派读
 *
 * @param ulAlias 别名
 * @param pucBuf  输出缓冲；usLen 非 0 时不得为 NULL
 * @param usLen   元素个数
 * @param ucType  保留，传 0
 * @return 成功返回传输字节数；失败返回负错误码
 */
int16_t dc_read_alias(uint32_t ulAlias, uint8_t *pucBuf, uint16_t usLen, uint8_t ucType)
{
    uint8_t ucClassId;
    /* 空缓冲且请求长度非 0 */
    if ((pucBuf == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    /* 按大类查读表并转入对应入口 */
    ucClassId = GetAliasClass(ulAlias);
    for (uint8_t i = 0u; i < (uint8_t)(sizeof(s_tReadAliasTable) / sizeof(s_tReadAliasTable[0])); i++) {
        if (ucClassId == s_tReadAliasTable[i].ucClassId) {
            return s_tReadAliasTable[i].entry(ulAlias, pucBuf, usLen, ucType);
        }
    }
    return DC_RET_ALIAS_ERR;
}

/**
 * @brief 按别名大类分派写
 *
 * @param ulAlias 别名
 * @param pucBuf  输入数据；usLen 非 0 时不得为 NULL
 * @param usLen   元素个数
 * @param ucType  保留，传 0
 * @return 成功返回传输字节数；失败返回负错误码
 */
int16_t dc_write_alias(uint32_t ulAlias, const uint8_t *pucBuf, uint16_t usLen, uint8_t ucType)
{
    uint8_t ucClassId;
    /* 空缓冲且请求长度非 0 */
    if ((pucBuf == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    /* 按大类查写表并转入对应入口 */
    ucClassId = GetAliasClass(ulAlias);
    for (uint8_t i = 0u; i < (uint8_t)(sizeof(s_tWriteAliasTable) / sizeof(s_tWriteAliasTable[0])); i++) {
        if (ucClassId == s_tWriteAliasTable[i].ucClassId) {
            return s_tWriteAliasTable[i].entry(ulAlias, pucBuf, usLen, ucType);
        }
    }
    return DC_RET_ALIAS_ERR;
}
