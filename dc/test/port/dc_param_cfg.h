/* Product param catalog and factory defaults. Edit per meter project. */
#ifndef DC_PARAM_CFG_H
#define DC_PARAM_CFG_H

#include <stdint.h>

/* Subclass IDs are assigned 0..N-1 by dc_param_pack (list order). */

static const uint8_t ucIntAttrTab[] = { DATATYPE_INT };

#define PARAM_HOLIDAY_TOTAL  (60u)
#define PARAM_HOLIDAY_K      (12u)
static const uint8_t ucHolidayAttribTab[] = {
    DATATYPE_ARRAY, 5u, PARAM_HOLIDAY_K,
};

#define PARAM_CALIB_TOTAL  (120u)
#define PARAM_CALIB_K      (12u)
static const uint8_t ucCalibAttribTab[] = {
    DATATYPE_LINKARRAY, 2u, 5u, PARAM_CALIB_K,
};

#define	REMOTECTRL_ILIMT		4		// 继电器拉闸电流门限
#define	REMOTECTRL_DELAY		2		// 超电流门限保护延时时间
#define	REMOTECTRL_INDEX	 2
static const uint8_t ucRemoteCtrlAttribTab[]=
{
    DATATYPE_STRUCT, REMOTECTRL_INDEX, REMOTECTRL_ILIMT, REMOTECTRL_DELAY,
};

#define PARAM_ITEM_LIST(X) \
    X(PARAM_REMOTECTRL,     DATATYPE_STRUCT,    6u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucRemoteCtrlAttribTab) \
    X(PARAM_SEASON_SWTIME,  DATATYPE_INT,       7u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucIntAttrTab) \
    X(PARAM_DAY_SWTIME,     DATATYPE_INT,       7u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucIntAttrTab) \
    X(PARAM_FEE_SWTIME,     DATATYPE_INT,       7u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucIntAttrTab) \
    X(PARAM_LADDER_SWTIME,  DATATYPE_INT,       7u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucIntAttrTab) \
    X(PARAM_HOLIDAY_DATA,   DATATYPE_ARRAY,     PARAM_HOLIDAY_TOTAL, (FLAG_ROM | FLAG_EEPROM), ucHolidayAttribTab) \
    X(PARAM_CALIB_DATA,     DATATYPE_LINKARRAY, PARAM_CALIB_TOTAL,   (FLAG_ROM | FLAG_EEPROM), ucCalibAttribTab) \
    X(PARAM_UN,             DATATYPE_INT,       4u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucIntAttrTab) \
    X(PARAM_IB,             DATATYPE_INT,       4u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucIntAttrTab) \
    X(PARAM_IMAX,           DATATYPE_INT,       4u,                (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK), ucIntAttrTab)



#if defined(DC_PARAM_PACK)

#define PARAM_ITEM_DEFAULTS(X) \
    X(PARAM_SEASON_SWTIME) \
    X(PARAM_DAY_SWTIME) \
    X(PARAM_FEE_SWTIME) \
    X(PARAM_LADDER_SWTIME)

static const uint8_t PARAM_SEASON_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};
static const uint8_t PARAM_DAY_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};
static const uint8_t PARAM_FEE_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};
static const uint8_t PARAM_LADDER_SWTIME_def[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 255u
};

#undef PARAM_FILL6

#endif /* DC_PARAM_PACK */

#endif
