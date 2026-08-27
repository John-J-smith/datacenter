/* Product variable catalog and EE layout. Edit per meter project. */
#ifndef DC_VARIABLE_CFG_H
#define DC_VARIABLE_CFG_H

#include "dc_storage_cfg.h"

#ifndef VAR_EEPROM_BASE
#define VAR_EEPROM_BASE DC_STORAGE_BASE_EE
#endif

#ifndef VAR_EE_BACKUP_BANKS
#define VAR_EE_BACKUP_BANKS 2
#endif

#if (VAR_EE_BACKUP_BANKS != 1) && (VAR_EE_BACKUP_BANKS != 2)
#error VAR_EE_BACKUP_BANKS must be 1 (single) or 2 (dual)
#endif

#ifndef VAR_A_BACKUP_INTERVAL_SEC
#define VAR_A_BACKUP_INTERVAL_SEC 600u
#endif
#ifndef VAR_B_BACKUP_INTERVAL_SEC
#define VAR_B_BACKUP_INTERVAL_SEC 600u
#endif
#ifndef VAR_PWR_DWN_INTERVAL_SEC
#define VAR_PWR_DWN_INTERVAL_SEC 720u
#endif

/* Subclass IDs are assigned 0..N-1 by dc_variable_pack (A, then B, C, D). */

#define VAR_LIST_A(X) \
    X(VARIABLE_DATE_TIME,     1u, 7u) \
    X(VARIABLE_RUN_TIME,      4u, 4u) \
    X(VARIABLE_WORK_TIME_BAT, 1u, 4u)

#define VAR_LIST_B(X) \
    X(VARIABLE_USED_MONTH,    1u, 4u) \
    X(VARIABLE_ENERGY_DEC,    1u, 16u) \
    X(VARIABLE_INTVENY_DEC,   1u, 8u)

#define VAR_LIST_C(X) \
    X(VARIABLE_RMS_VOLTAGE,      3u, 2u) \
    X(VARIABLE_RMS_CURRENT,      5u, 4u) \
    X(VARIABLE_VOLT_ANGLE,       3u, 2u) \
    X(VARIABLE_PHASE_ANGLE,      3u, 2u) \
    X(VARIABLE_ACTIVE_POWER,     4u, 4u) \
    X(VARIABLE_REACTIVE_POWER,   4u, 4u) \
    X(VARIABLE_APPARENT_POWER,   4u, 4u) \
    X(VARIABLE_ACTPOW_PERMIN,    4u, 4u) \
    X(VARIABLE_REACTPOW_PERMIN,  4u, 4u) \
    X(VARIABLE_POWER_FACT,       4u, 2u) \
    X(VARIABLE_POWER_FREQ,       1u, 2u) \
    X(VARIABLE_METER_TMP,        1u, 2u) \
    X(VARIABLE_VOLT_BATTIM,      1u, 2u) \
    X(VARIABLE_VOLT_BATDIS,      1u, 2u) \
    X(VARIABLE_STAWDS_METER,     7u, 2u) \
    X(VARIABLE_STAWDS_FLRPT,     1u, 4u) \
    X(VARIABLE_DEMAND_ACTIVE,    1u, 4u) \
    X(VARIABLE_DEMAND_REACTIVE,  1u, 4u) \
    X(VARIABLE_WORK_FEENO,       1u, 1u) \
    X(VARIABLE_RTC_SECMIN,       2u, 4u)

#define VAR_LIST_D(X) \
    X(VARIABLE_MTWORK_EVTKEY, 1u, 8u)

#endif
