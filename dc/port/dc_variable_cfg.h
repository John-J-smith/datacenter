/* Copy to product port/ as dc_variable_cfg.h and edit VAR_LIST_* for your meter. */
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

#define VAR_LIST_A(X) \
    X(VARIABLE_DATE_TIME, 1u, 7u)

#define VAR_LIST_B(X) \
    X(VARIABLE_USED_MONTH, 1u, 4u)

#define VAR_LIST_C(X) \
    X(VARIABLE_RMS_VOLTAGE, 3u, 2u)

#define VAR_LIST_D(X) \
    X(VARIABLE_MTWORK_EVTKEY, 1u, 8u)

#endif
