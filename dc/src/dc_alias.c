#include "datacenter.h"
#include "dc_entry.h"

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, uint8_t *, uint16_t, uint8_t);
} ST_ALIAS_RSTORAGE_TABLE;

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, const uint8_t *, uint16_t, uint8_t);
} ST_ALIAS_WSTORAGE_TABLE;

static const ST_ALIAS_RSTORAGE_TABLE readAliasDataTable[] = {
    { (uint8_t)ALIAS_CLASS_ENERGY,    dc_read_energy },
    { (uint8_t)ALIAS_CLASS_DEMAND,    dc_read_demand },
    { (uint8_t)ALIAS_CLASS_PARAMETER, dc_read_param },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  dc_read_variable },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, dc_read_list },
    { (uint8_t)ALIAS_CLASS_RECORD,    dc_read_record },
};

static const ST_ALIAS_WSTORAGE_TABLE writeAliasDataTable[] = {
    { (uint8_t)ALIAS_CLASS_DEMAND,    dc_write_demand },
    { (uint8_t)ALIAS_CLASS_PARAMETER, dc_write_param },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  dc_write_variable },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, dc_write_list },
    { (uint8_t)ALIAS_CLASS_RECORD,    dc_write_record },
};

int16_t dc_read_alias(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    uint8_t class_id;
    uint8_t i;

    if ((dataPtr == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    class_id = GetAliasClass(alias);
    for (i = 0u; i < (uint8_t)(sizeof(readAliasDataTable) / sizeof(readAliasDataTable[0])); i++) {
        if (class_id == readAliasDataTable[i].ucClassId) {
            return readAliasDataTable[i].entry(alias, dataPtr, usLen, type);
        }
    }
    return DC_RET_ALIAS_ERR;
}

int16_t dc_write_alias(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    uint8_t class_id;
    uint8_t i;

    if ((dataPtr == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    class_id = GetAliasClass(alias);
    for (i = 0u; i < (uint8_t)(sizeof(writeAliasDataTable) / sizeof(writeAliasDataTable[0])); i++) {
        if (class_id == writeAliasDataTable[i].ucClassId) {
            return writeAliasDataTable[i].entry(alias, dataPtr, usLen, type);
        }
    }
    return DC_RET_ALIAS_ERR;
}
