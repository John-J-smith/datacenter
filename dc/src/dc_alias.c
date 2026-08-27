#include "datacenter.h"
#include "dc_entry.h"

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, uint8_t *, uint16_t, uint8_t);
} STR_ALIAS_RSTORAGE_TABLE;

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, const uint8_t *, uint16_t, uint8_t);
} STR_ALIAS_WSTORAGE_TABLE;

static const STR_ALIAS_RSTORAGE_TABLE readAliasDataTable[] = {
    { (uint8_t)ALIAS_CLASS_ENERGY,    ReadEnergyData },
    { (uint8_t)ALIAS_CLASS_DEMAND,    ReadDemandData },
    { (uint8_t)ALIAS_CLASS_PARAMETER, ReadParamData },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  ReadVariableData },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, ReadListParamData },
    { (uint8_t)ALIAS_CLASS_RECORD,    ReadRecordData },
};

static const STR_ALIAS_WSTORAGE_TABLE writeAliasDataTable[] = {
    { (uint8_t)ALIAS_CLASS_DEMAND,    WriteDemandData },
    { (uint8_t)ALIAS_CLASS_PARAMETER, WriteParamData },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  WriteVariableData },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, WriteListParamData },
    { (uint8_t)ALIAS_CLASS_RECORD,    WriteRecordData },
};

int16_t ReadAliasData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    uint8_t class_id;
    uint8_t i;

    if ((dataPtr == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    class_id = GetAliasClass(genre);
    for (i = 0u; i < (uint8_t)(sizeof(readAliasDataTable) / sizeof(readAliasDataTable[0])); i++) {
        if (class_id == readAliasDataTable[i].ucClassId) {
            return readAliasDataTable[i].entry(genre, dataPtr, usLen, type);
        }
    }
    return DC_RET_ALIAS_ERR;
}

int16_t WriteAliasData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    uint8_t class_id;
    uint8_t i;

    if ((dataPtr == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    class_id = GetAliasClass(genre);
    for (i = 0u; i < (uint8_t)(sizeof(writeAliasDataTable) / sizeof(writeAliasDataTable[0])); i++) {
        if (class_id == writeAliasDataTable[i].ucClassId) {
            return writeAliasDataTable[i].entry(genre, dataPtr, usLen, type);
        }
    }
    return DC_RET_ALIAS_ERR;
}
