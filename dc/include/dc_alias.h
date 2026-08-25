#ifndef DC_ALIAS_H
#define DC_ALIAS_H

#include <stdint.h>

typedef enum {
    ALIAS_CLASS_ENERGY = 0,
    ALIAS_CLASS_DEMAND = 1,
    ALIAS_CLASS_PARAMETER = 2,
    ALIAS_CLASS_VARIABLE = 3,
    ALIAS_CLASS_LISTPARAM = 4,
    ALIAS_CLASS_RECORD = 5
} E_ALIAS_CLASS;

#define GetAliasClass(a)     ((uint8_t)(((uint32_t)(a) >> 24) & 0xFFu))
#define ParaAliasToType(a)   ((uint16_t)(((uint32_t)(a) >> 8) & 0xFFFFu))
#define GetAliasIndex(a)     ((uint8_t)((uint32_t)(a) & 0xFFu))

#define EnergyAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_ENERGY) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define DemandAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_DEMAND) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define ParaAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_PARAMETER) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define VarAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_VARIABLE) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define ListParaAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_LISTPARAM) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define RecordAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_RECORD) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))

#endif
