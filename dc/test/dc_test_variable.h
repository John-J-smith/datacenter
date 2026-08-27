#ifndef DC_TEST_VARIABLE_H
#define DC_TEST_VARIABLE_H

#include <stdint.h>

typedef enum {
    DC_TEST_VAR_ZONE_A = 0,
    DC_TEST_VAR_ZONE_B = 1,
} dc_test_var_zone_t;

void DcTestVarReset(void);
void DcTestVarCorruptMagic(dc_test_var_zone_t zone);
void DcTestVarCorruptCrc(dc_test_var_zone_t zone);
void DcTestVarInvalidateAll(dc_test_var_zone_t zone);
void DcTestVarResetBackupTimers(void);

#endif
