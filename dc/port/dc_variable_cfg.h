/* Product variable catalog and EE layout. Edit per meter project. */
#ifndef DC_VARIABLE_CFG_H
#define DC_VARIABLE_CFG_H

#include "dc_storage_cfg.h"

/* Default VAR_EEPROM_BASE follows unified EE origin unless overridden. */
#ifndef VAR_EEPROM_BASE
#define VAR_EEPROM_BASE DC_STORAGE_BASE_EE
#endif

/*
 * Timed EE backup banks for A/B (design 7.4.2.1):
 *   1 = single: one PWR_ON bank + one PWR_DWN bank per class
 *   2 = dual:   two PWR_ON banks (written together) + one PWR_DWN bank
 */
#ifndef VAR_EE_BACKUP_BANKS
#define VAR_EE_BACKUP_BANKS 2
#endif

#if (VAR_EE_BACKUP_BANKS != 1) && (VAR_EE_BACKUP_BANKS != 2)
#error VAR_EE_BACKUP_BANKS must be 1 (single) or 2 (dual)
#endif

/* Periodic backup intervals (seconds). */
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
    X(VARIABLE_DATE_TIME,     0x4000u, 1u, DC_VAR_CALENDAR_BYTES) \
    X(VARIABLE_RUN_TIME,      0xE000u, 4u, 4u) \
    X(VARIABLE_WORK_TIME_BAT, 0x2013u, 1u, 4u)

#define VAR_LIST_B(X) \
    X(VARIABLE_USED_MONTH,    0x2031u, 1u, 4u) \
    X(VARIABLE_ENERGY_DEC,    0xE007u, 1u, DC_VAR_ENERGY_PULSE_BYTES) \
    X(VARIABLE_INTVENY_DEC,   0xE008u, 1u, DC_VAR_INTERVAL_PULSE_BYTES)

#define VAR_LIST_C(X) \
    X(VARIABLE_RMS_VOLTAGE,      0x2000u, 3u, 2u) \
    X(VARIABLE_RMS_CURRENT,      0x2001u, 5u, 4u) \
    X(VARIABLE_VOLT_ANGLE,       0x2002u, 3u, 2u) \
    X(VARIABLE_PHASE_ANGLE,      0x2003u, 3u, 2u) \
    X(VARIABLE_ACTIVE_POWER,     0x2004u, 4u, 4u) \
    X(VARIABLE_REACTIVE_POWER,   0x2005u, 4u, 4u) \
    X(VARIABLE_APPARENT_POWER,   0x2006u, 4u, 4u) \
    X(VARIABLE_ACTPOW_PERMIN,    0x2007u, 4u, 4u) \
    X(VARIABLE_REACTPOW_PERMIN,  0x2008u, 4u, 4u) \
    X(VARIABLE_POWER_FACT,       0x200Au, 4u, 2u) \
    X(VARIABLE_POWER_FREQ,       0x200Fu, 1u, 2u) \
    X(VARIABLE_METER_TMP,        0x2010u, 1u, 2u) \
    X(VARIABLE_VOLT_BATTIM,      0x2011u, 1u, 2u) \
    X(VARIABLE_VOLT_BATDIS,      0x2012u, 1u, 2u) \
    X(VARIABLE_STAWDS_METER,     0x2014u, 7u, 2u) \
    X(VARIABLE_STAWDS_FLRPT,     0x2015u, 1u, 4u) \
    X(VARIABLE_DEMAND_ACTIVE,    0x2017u, 1u, 4u) \
    X(VARIABLE_DEMAND_REACTIVE,  0x2018u, 1u, 4u) \
    X(VARIABLE_WORK_FEENO,       0xE003u, 1u, 1u) \
    X(VARIABLE_RTC_SECMIN,       0xE205u, 2u, 4u)

#define VAR_LIST_D(X) \
    X(VARIABLE_MTWORK_EVTKEY, 0xE100u, 1u, DC_VAR_KEY_BYTES)

#endif
