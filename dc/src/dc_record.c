#include "dc_entry.h"
#include "datacenter.h"

int16_t ReadRecordData(uint32_t genre, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)genre;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}

int16_t WriteRecordData(uint32_t genre, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)genre;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}
