#ifndef DC_TEST_STORAGE_H
#define DC_TEST_STORAGE_H

#include <stdint.h>

void DcTestStorageReset(void);
void DcTestStorageFailNextWrites(uint16_t usCount);
uint8_t *DcTestStoragePtr(void);

#endif
