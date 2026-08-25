#ifndef DC_ENTRY_H
#define DC_ENTRY_H

#include <stdint.h>

int16_t ReadEnergyData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t ReadDemandData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t WriteDemandData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t ReadParamData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t WriteParamData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t ReadVariableData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t WriteVariableData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t ReadListParamData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t WriteListParamData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t ReadRecordData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t WriteRecordData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

#endif
