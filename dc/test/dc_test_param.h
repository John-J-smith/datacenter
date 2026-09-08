#ifndef DC_TEST_PARAM_H
#define DC_TEST_PARAM_H

#include <stdint.h>

void DcTestParamReset(void);
void DcTestParamReinit(void);
void DcTestParamCorruptSramMagic(void);
void DcTestParamCorruptBlockCrc(uint8_t ucBlk);
uint8_t DcTestParamSramOk(void);

#endif
